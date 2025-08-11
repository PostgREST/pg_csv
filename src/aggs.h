#ifndef AGGS_H
#define AGGS_H

typedef struct {
  char delim;
  bool with_bom;
  bool header;
} CsvOptions;

typedef struct {
  StringInfoData accum_buf;
  bool           header_done;
  bool           first_row;
  TupleDesc      tupdesc;
  CsvOptions    *options;
} CsvAggState;

void parse_csv_options(HeapTupleHeader opts_hdr, CsvOptions *csv_opts);

void csv_append_field(StringInfo buf, const char *s, size_t n, char delim);

#endif
