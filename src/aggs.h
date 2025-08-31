#ifndef AGGS_H
#define AGGS_H

// mirrors the SQL csv_options type
typedef struct {
  char delimiter;
  bool bom;
  bool header;
} CsvOptions;
#define csv_options_count 3

typedef struct {
  StringInfoData accum_buf;
  bool           header_done;
  bool           first_row;
  TupleDesc      tupdesc;
  CsvOptions    *options;
} CsvAggState;

extern const char NEWLINE;
extern const char BOM[3];
extern const char DQUOTE;
extern const char CR;

void parse_csv_options(HeapTupleHeader opts_hdr, CsvOptions *csv_opts);

void csv_append_field(StringInfo buf, const char *s, size_t n, char delim);

#endif
