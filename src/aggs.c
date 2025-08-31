// Helpers for the top module

#include "pg_prelude.h"

#include "aggs.h"

const char NEWLINE = '\n';
const char DQUOTE  = '"';
const char CR      = '\r';
const char BOM[3]  = "\xEF\xBB\xBF";

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

void parse_csv_options(HeapTupleHeader opts_hdr, CsvOptions *csv_opts) {
  // defaults
  csv_opts->delim    = ',';
  csv_opts->with_bom = false;
  csv_opts->header   = true;

  if (opts_hdr == NULL) return;

  TupleDesc desc = lookup_rowtype_tupdesc(HeapTupleHeaderGetTypeId(opts_hdr),
                                          HeapTupleHeaderGetTypMod(opts_hdr));

  Datum values[3];
  bool  nulls[3];

  heap_deform_tuple(
      &(HeapTupleData){.t_len = HeapTupleHeaderGetDatumLength(opts_hdr), .t_data = opts_hdr}, desc,
      values, nulls);

  if (!nulls[0]) {
    csv_opts->delim = DatumGetChar(values[0]);
    if (is_reserved(csv_opts->delim))
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("delimiter cannot be newline, carriage return or "
                             "double quote")));
  }

  if (!nulls[1]) {
    csv_opts->with_bom = DatumGetBool(values[1]);
  }

  if (!nulls[2]) {
    csv_opts->header = DatumGetBool(values[2]);
  }

  ReleaseTupleDesc(desc);
}

void csv_append_field(StringInfo buf, const char *s, size_t n, char delim) {
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
