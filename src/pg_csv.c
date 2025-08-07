#include "pg_prelude.h"

PG_MODULE_MAGIC;

static const char NEWLINE = '\n';
static const char DQUOTE  = '"';
static const char CR      = '\r';

typedef struct {
  StringInfoData accum_buf;
  bool           header_done;
  bool           first_row;
  TupleDesc      tupdesc;
} CsvAggState;

static inline bool is_reserved(char c) {
  return c == DQUOTE || c == NEWLINE || c == CR;
}

// Any comma, quote, CR, LF requires quoting as per RFC https://www.ietf.org/rfc/rfc4180.txt
static inline bool needs_quote(const char *s, size_t n, char delim) {
  while (n--) {
    char c = *s++;
    if (c == delim || is_reserved(c)) return true;
  }
  return false;
}

static inline void csv_append_field(StringInfo buf, const char *s, size_t n, char delim) {
  if (!needs_quote(s, n, delim)) {
    appendBinaryStringInfo(buf, s, n);
  } else {
    appendStringInfoChar(buf, DQUOTE);
    for (size_t j = 0; j < n; j++) {
      char c = s[j];
      if (c == DQUOTE) appendStringInfoChar(buf, DQUOTE);
      appendStringInfoChar(buf, c);
    }
    appendStringInfoChar(buf, DQUOTE);
  }
}

static char *datum_to_cstring(Datum datum, Oid typeoid) {
  Oid  out_func;
  bool is_varlena;
  getTypeOutputInfo(typeoid, &out_func, &is_varlena);

  return OidOutputFunctionCall(out_func, datum);
}

PG_FUNCTION_INFO_V1(csv_agg_transfn);
Datum csv_agg_transfn(PG_FUNCTION_ARGS) {
  CsvAggState *state;

  // first call when the accumulator is NULL
  // pretty standard stuff, for example see the jsonb_agg transition function
  // https://github.com/postgres/postgres/blob/3c4e26a62c31ebe296e3aedb13ac51a7a35103bd/src/backend/utils/adt/jsonb.c#L1521
  if (PG_ARGISNULL(0)) {
    MemoryContext aggctx, oldctx;

    if (!AggCheckCallContext(fcinfo, &aggctx))
      elog(ERROR, "csv_agg_transfn called in non‑aggregate context");

    oldctx = MemoryContextSwitchTo(aggctx);

    state = (CsvAggState *)palloc(sizeof(CsvAggState));
    initStringInfo(&state->accum_buf);
    state->header_done = false;
    state->first_row   = true;
    state->tupdesc     = NULL;

    MemoryContextSwitchTo(oldctx);
  } else
    state = (CsvAggState *)PG_GETARG_POINTER(0);

  if (PG_ARGISNULL(1)) PG_RETURN_POINTER(state); // skip NULL rows

  HeapTupleHeader next = PG_GETARG_HEAPTUPLEHEADER(1);

  char delim = PG_NARGS() >= 3 && !PG_ARGISNULL(2) ? PG_GETARG_CHAR(2) : ',';

  if (is_reserved(delim))
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                    errmsg("delimiter cannot be newline, carriage return or double quote")));

  // build header and cache tupdesc once
  if (!state->header_done) {
    TupleDesc tdesc =
        lookup_rowtype_tupdesc(HeapTupleHeaderGetTypeId(next), HeapTupleHeaderGetTypMod(next));

    // build header row
    for (int i = 0; i < tdesc->natts; i++) {
      Form_pg_attribute att = TupleDescAttr(tdesc, i);
      if (att->attisdropped) // pg always keeps dropped columns, guard against this
        continue;

      if (i > 0) // only append delimiter after the first value
        appendStringInfoChar(&state->accum_buf, delim);

      char *cstr = NameStr(att->attname);
      csv_append_field(&state->accum_buf, cstr, strlen(cstr), delim);
    }

    appendStringInfoChar(&state->accum_buf, NEWLINE);

    state->tupdesc     = tdesc;
    state->header_done = true;
  }

  // build body
  int tuple_natts = state->tupdesc->natts;

  Datum *datums = (Datum *)palloc(tuple_natts * sizeof(Datum));
  bool  *nulls  = (bool *)palloc(tuple_natts * sizeof(bool));

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

    if (i > 0) appendStringInfoChar(&state->accum_buf, delim);

    if (nulls[i]) continue; // empty field for NULL

    char *cstr = datum_to_cstring(datums[i], att->atttypid);
    csv_append_field(&state->accum_buf, cstr, strlen(cstr), delim);
  }

  PG_RETURN_POINTER(state);
}

PG_FUNCTION_INFO_V1(csv_agg_finalfn);
Datum csv_agg_finalfn(PG_FUNCTION_ARGS) {
  if (PG_ARGISNULL(0)) PG_RETURN_NULL();

  CsvAggState *state = (CsvAggState *)PG_GETARG_POINTER(0);

  if (state->tupdesc != NULL) ReleaseTupleDesc(state->tupdesc);

  PG_RETURN_TEXT_P(cstring_to_text_with_len(state->accum_buf.data, state->accum_buf.len));
}
