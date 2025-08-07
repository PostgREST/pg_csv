alter type csv_options add attribute header bool;

create or replace function csv_options(
  delimiter "char" default NULL,
  bom       bool default NULL,
  header    bool default NULL
) returns csv_options as $$
  select row(delimiter, bom, header)::csv_options;
$$ language sql;
