// This is the top module, all SQL exposed functions will be in this file

#define PG_PRELUDE_IMPL
#include "pg_prelude.h"

#include "aggs.h"
#include "csv.h"

PGDLLEXPORT const Pg_magic_struct *Pg_magic_func(void);
PG_MODULE_MAGIC;

static int count_visible_columns(TupleDesc tupdesc) {
  int visible = 0;

  for (int i = 0; i < tupdesc->natts; i++) {
    if (!TupleDescAttr(tupdesc, i)->attisdropped) visible++;
  }

  return visible;
}

// We need a cstring to pass to functions like InputFunctionCall, as unfortunately it doesn't allow
// passing a length. To avoid copying into a new buffer and appending it a `\0`, we take advantage
// of some facts:
// - The slice always point into a mutable input buffer
// - The byte immediately after the slice is still within that same buffer. either a csv
//   delimiter/newline or the trailing '\0' from text_to_cstring().
static char *csv_slice_to_cstr(CpcSlice field) {
  char *cstr      = (char *)field.ptr;
  cstr[field.len] = '\0';
  return cstr;
}

static bool attname_matches_cpc_slice(const char *attname, CpcSlice field) {
  // This saves us calling strlen on the attname to compare it to the slice
  return field.len < NAMEDATALEN && attname[field.len] == '\0' &&
         pg_strncasecmp(attname, field.ptr, field.len) == 0;
}

// aggregate final function
PG_FUNCTION_INFO_V1(csv_agg_finalfn);
Datum csv_agg_finalfn(PG_FUNCTION_ARGS) {
  if (PG_ARGISNULL(0)) PG_RETURN_NULL();

  CsvAggState *state = (CsvAggState *)PG_GETARG_POINTER(0);

  if (state->tupdesc != NULL) ReleaseTupleDesc(state->tupdesc);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(state->accum_buf.data, state->accum_buf.len));
}

// aggregate transition function
PG_FUNCTION_INFO_V1(csv_agg_transfn);
Datum csv_agg_transfn(PG_FUNCTION_ARGS) {
  CsvAggState    *state = !PG_ARGISNULL(0) ? (CsvAggState *)PG_GETARG_POINTER(0) : NULL;
  HeapTupleHeader next  = !PG_ARGISNULL(1) ? PG_GETARG_HEAPTUPLEHEADER(1) : NULL;

  // first call when the accumulator is NULL
  // pretty standard stuff, for example see the jsonb_agg transition function
  // https://github.com/postgres/postgres/blob/3c4e26a62c31ebe296e3aedb13ac51a7a35103bd/src/backend/utils/adt/jsonb.c#L1521
  if (state == NULL) {
    MemoryContext aggctx, oldctx;

    if (!AggCheckCallContext(fcinfo, &aggctx))
      elog(ERROR, "%s called in non‑aggregate context", __func__);

    // here we extend the lifetime of the CsvAggState until the aggregate finishes
    oldctx = MemoryContextSwitchTo(aggctx);

    state = palloc(sizeof(CsvAggState));
    initStringInfo(&state->accum_buf);
    state->header_done    = false;
    state->first_row      = true;
    state->tupdesc        = NULL;
    state->nullstr_len    = 0;
    state->cached_nullstr = NULL;
    state->options        = palloc(sizeof(CsvOptions));

    // we'll parse the csv options only once
    HeapTupleHeader opts_hdr =
        PG_NARGS() >= 3 && !PG_ARGISNULL(2) ? PG_GETARG_HEAPTUPLEHEADER(2) : NULL;
    parse_csv_options(opts_hdr, state->options);

    if (state->options->nullstr) {
      state->cached_nullstr = text_to_cstring(state->options->nullstr);
      state->nullstr_len    = VARSIZE_ANY_EXHDR(state->options->nullstr);
    }

    MemoryContextSwitchTo(oldctx);
  }

  if (next == NULL) PG_RETURN_POINTER(state); // skip NULL rows

  // build header and cache tupdesc once
  if (!state->header_done) {
    TupleDesc tdesc =
        lookup_rowtype_tupdesc(HeapTupleHeaderGetTypeId(next), HeapTupleHeaderGetTypMod(next));

    if (state->options->bom) appendBinaryStringInfo(&state->accum_buf, BOM, sizeof(BOM));

    // build header row
    if (state->options->header) {
      for (int i = 0; i < tdesc->natts; i++) {
        Form_pg_attribute att = TupleDescAttr(tdesc, i);
        if (att->attisdropped) // pg always keeps dropped columns, guard against this
          continue;

        if (i > 0) // only append delimiter after the first value
          appendStringInfoChar(&state->accum_buf, state->options->delimiter);

        char *cstr = NameStr(att->attname);
        csv_append_field(&state->accum_buf, cstr, strlen(cstr), state->options->delimiter);
      }

      appendStringInfoChar(&state->accum_buf, NEWLINE);
    }

    state->tupdesc     = tdesc;
    state->header_done = true;
  }

  // build body
  int tuple_natts = state->tupdesc->natts;

  Datum *datums = (Datum *)palloc(mul_size(tuple_natts, sizeof(Datum)));
  bool  *nulls  = (bool *)palloc(mul_size(tuple_natts, sizeof(bool)));

  // extract the values of the next row
  heap_deform_tuple(
      &(HeapTupleData){
        .t_len  = HeapTupleHeaderGetDatumLength(next),
        .t_data = next,
      },
      state->tupdesc, datums, nulls);

  // newline before every data row except the first
  // we do this to avoid trimming the last newline once we're done with all rows
  if (!state->first_row) appendStringInfoChar(&state->accum_buf, NEWLINE);
  state->first_row = false;

  // create next row
  for (int i = 0; i < tuple_natts; i++) {
    Form_pg_attribute att = TupleDescAttr(state->tupdesc, i);
    if (att->attisdropped) // pg always keeps dropped columns, guard against this
      continue;

    if (i > 0) appendStringInfoChar(&state->accum_buf, state->options->delimiter);

    if (nulls[i]) {
      if (state->cached_nullstr)
        csv_append_field(&state->accum_buf, state->cached_nullstr, state->nullstr_len,
                         state->options->delimiter);
    } else {
      char *cstr = datum_to_cstring(datums[i], att->atttypid);
      csv_append_field(&state->accum_buf, cstr, strlen(cstr), state->options->delimiter);
    }
  }

  PG_RETURN_POINTER(state);
}

PG_FUNCTION_INFO_V1(csv_populate_recordset);
Datum csv_populate_recordset(PG_FUNCTION_ARGS) {
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;

  if (rsinfo == NULL || !(rsinfo->allowedModes & SFRM_Materialize))
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required")));

  Oid base_type = get_fn_expr_argtype(fcinfo->flinfo, 0);
  if (!OidIsValid(base_type))
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                    errmsg("could not determine row type from first argument")));

  if (PG_ARGISNULL(1))
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("from_csv must not be NULL")));

  bool has_header = false;
  if (PG_NARGS() >= 3) {
    if (PG_ARGISNULL(2))
      ereport(ERROR,
              (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("has_header must not be NULL")));

    has_header = PG_GETARG_BOOL(2);
  }

  TupleDesc srcdesc = lookup_rowtype_tupdesc(base_type, -1);
  TupleDesc outdesc = BlessTupleDesc(CreateTupleDescCopy(srcdesc));
  int       natts   = srcdesc->natts;
  int       vnatts  = count_visible_columns(srcdesc);

  if (vnatts == 0)
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("row type has no columns")));

  FmgrInfo *infuncs  = palloc0(mul_size(natts, sizeof(FmgrInfo)));
  Oid      *ioparams = palloc0(mul_size(natts, sizeof(Oid)));
  int32    *typmods  = palloc0(mul_size(natts, sizeof(int32)));

  for (int i = 0; i < natts; i++) {
    Form_pg_attribute att = TupleDescAttr(srcdesc, i);
    Oid               inoid;

    if (att->attisdropped) continue;

    getTypeInputInfo(att->atttypid, &inoid, &ioparams[i]);
    fmgr_info(inoid, &infuncs[i]);
    typmods[i] = att->atttypmod;
  }

  int *visible_attnos = palloc(mul_size(vnatts, sizeof(int)));
  int  visible_idx    = 0;

  for (int i = 0; i < natts; i++) {
    if (!TupleDescAttr(srcdesc, i)->attisdropped) visible_attnos[visible_idx++] = i;
  }

  MemoryContext per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  MemoryContext oldctx        = MemoryContextSwitchTo(per_query_ctx);

  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setDesc    = outdesc;
  rsinfo->setResult  = tuplestore_begin_heap(true, false, work_mem);

  // Parse the input text row by row, then materialize each row into the tuplestore.
  CpcSlice remaining = cpc_slice_from_cstr(text_to_cstring(PG_GETARG_TEXT_PP(1)));

  // MaxHeapAttributeNumber is the maximum number of columns a table can have
  CpcValue      arena_items[MaxHeapAttributeNumber];
  CpcArena      arena       = {0};
  int          *header_map  = NULL;
  int           header_cols = 0;
  Datum        *values      = palloc(mul_size(natts, sizeof(Datum)));
  bool         *nulls       = palloc(mul_size(natts, sizeof(bool)));
  MemoryContext row_ctx =
      AllocSetContextCreate(per_query_ctx, "csv_populate_recordset row", ALLOCSET_DEFAULT_SIZES);
  int64 row_number = 0;

  cpc_arena_init(&arena, arena_items, MaxHeapAttributeNumber, NULL);

  while (remaining.len > 0) {
    // Reuse the arena for each csv row
    cpc_arena_reset(&arena);

    CpcResult parsed = CPC_PARSE(csvRow, remaining, &arena);

    if (!parsed.ok) {
      if (parsed.err.kind == CPC_ERR_ARENA_FULL)
        ereport(ERROR,
                (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
                 errmsg("CSV row has more than %d columns, which is the maximum in postgres.",
                        MaxHeapAttributeNumber)));

      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                      errmsg("invalid CSV at row %lld: %s", (long long)(row_number + 1),
                             parsed.err.msg ? parsed.err.msg : "parse error")));
    }

    row_number++;
    remaining = parsed.rest;

    if (!cpc_is_list(&parsed.out))
      ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("unexpected parser output")));

    size_t field_count = parsed.out.as.list.len;

    // The first parsed row can map a column name to attribute number
    if (has_header && header_map == NULL) {
      bool *seen;

      header_cols = (int)field_count;
      header_map  = MemoryContextAlloc(per_query_ctx, mul_size(header_cols, sizeof(int)));
      seen        = palloc0(mul_size(natts, sizeof(bool)));

      for (int col = 0; col < header_cols; col++) {
        const CpcValue *field = cpc_val_list_at(&arena, &parsed.out, col);
        int             match = -1;

        if (field == NULL || !cpc_is_slice(field))
          ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("missing header column data")));

        if (field->as.slice.len == 0)
          ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
                          errmsg("header column %d is empty", col + 1)));

        for (int i = 0; i < natts; i++) {
          Form_pg_attribute att = TupleDescAttr(srcdesc, i);

          if (att->attisdropped) continue;

          if (attname_matches_cpc_slice(NameStr(att->attname), field->as.slice)) {
            match = i;
            break;
          }
        }

        if (match < 0)
          ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
                          errmsg("unknown column \"%.*s\" in header", (int)field->as.slice.len,
                                 field->as.slice.ptr)));
        if (seen[match])
          ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
                          errmsg("duplicate column \"%.*s\" in header", (int)field->as.slice.len,
                                 field->as.slice.ptr)));

        seen[match]     = true;
        header_map[col] = match;
      }

      continue;
    }

    if (field_count != (size_t)(has_header ? header_cols : vnatts))
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                      errmsg("row %lld has %zu columns, expected %d", (long long)row_number,
                             field_count, has_header ? header_cols : vnatts)));

    // Convert the parsed field slices into typed Datums for one output tuple
    MemoryContextReset(row_ctx);
    MemoryContextSwitchTo(row_ctx);

    for (int i = 0; i < natts; i++) {
      values[i] = (Datum)0;
      nulls[i]  = true;
    }

    for (int col = 0; col < (int)field_count; col++) {
      const CpcValue *field     = cpc_val_list_at(&arena, &parsed.out, col);
      int             att_index = has_header ? header_map[col] : visible_attnos[col];

      if (field == NULL || !cpc_is_slice(field))
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("missing row column data")));

      if (field->as.slice.len == 0) continue;

      values[att_index] = InputFunctionCall(&infuncs[att_index], csv_slice_to_cstr(field->as.slice),
                                            ioparams[att_index], typmods[att_index]);
      nulls[att_index]  = false;
    }

    // The tuple values must survive long enough to be copied into the tuplestore
    MemoryContextSwitchTo(per_query_ctx);
    tuplestore_putvalues(rsinfo->setResult, outdesc, values, nulls);
  }

  MemoryContextDelete(row_ctx);
  MemoryContextSwitchTo(oldctx);
  ReleaseTupleDesc(srcdesc);

  return (Datum)0;
}
