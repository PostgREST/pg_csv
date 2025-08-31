// This is the top module, all SQL exposed functions will be in this file

#define PG_PRELUDE_IMPL
#include "pg_prelude.h"

#include "aggs.h"

PG_MODULE_MAGIC;

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
    state->header_done = false;
    state->first_row   = true;
    state->tupdesc     = NULL;
    state->options     = palloc(sizeof(CsvOptions));

    // we'll parse the csv options only once
    HeapTupleHeader opts_hdr =
        PG_NARGS() >= 3 && !PG_ARGISNULL(2) ? PG_GETARG_HEAPTUPLEHEADER(2) : NULL;
    parse_csv_options(opts_hdr, state->options);

    MemoryContextSwitchTo(oldctx);
  }

  if (next == NULL) PG_RETURN_POINTER(state); // skip NULL rows

  // build header and cache tupdesc once
  if (!state->header_done) {
    TupleDesc tdesc =
        lookup_rowtype_tupdesc(HeapTupleHeaderGetTypeId(next), HeapTupleHeaderGetTypMod(next));

    if (state->options->with_bom) appendBinaryStringInfo(&state->accum_buf, BOM, sizeof(BOM));

    // build header row
    if (state->options->header) {
      for (int i = 0; i < tdesc->natts; i++) {
        Form_pg_attribute att = TupleDescAttr(tdesc, i);
        if (att->attisdropped) // pg always keeps dropped columns, guard against this
          continue;

        if (i > 0) // only append delimiter after the first value
          appendStringInfoChar(&state->accum_buf, state->options->delim);

        char *cstr = NameStr(att->attname);
        csv_append_field(&state->accum_buf, cstr, strlen(cstr), state->options->delim);
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

    if (i > 0) appendStringInfoChar(&state->accum_buf, state->options->delim);

    if (nulls[i]) continue; // empty field for NULL

    char *cstr = datum_to_cstring(datums[i], att->atttypid);
    csv_append_field(&state->accum_buf, cstr, strlen(cstr), state->options->delim);
  }

  PG_RETURN_POINTER(state);
}
