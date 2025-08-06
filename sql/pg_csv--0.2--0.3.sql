alter type csv_options add attribute bom bool;

create or replace function csv_options(delimiter "char" default NULL, bom bool default NULL) returns csv_options as $$
  select row(delimiter, bom)::csv_options;
$$ language sql;
