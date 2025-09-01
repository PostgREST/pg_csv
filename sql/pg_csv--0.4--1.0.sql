alter type csv_options add attribute nullstr text;

create or replace function csv_options
( delimiter "char" default NULL
, bom       bool default NULL
, header    bool default NULL
, nullstr   text default NULL
) returns csv_options as $$
  select row(delimiter, bom, header, nullstr)::csv_options;
$$ language sql;

create or replace function csv_agg_transfn(internal, anyelement)
  returns internal
  language c
as 'MODULE_PATHNAME';

create or replace function csv_agg_transfn(internal, anyelement, csv_options)
  returns internal
  language c
as 'MODULE_PATHNAME';

create or replace function csv_agg_finalfn(internal)
  returns text
  language c
as 'MODULE_PATHNAME';
