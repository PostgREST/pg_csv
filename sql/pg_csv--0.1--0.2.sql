drop aggregate if exists csv_agg (anyelement, "char");
drop function  if exists csv_agg_transfn (internal, anyelement, "char");

create type csv_options as (
  delimiter "char"
);

create function csv_options(delimiter "char" default ',') returns csv_options as $$
  select row(delimiter)::csv_options;
$$ language sql;

create function csv_agg_transfn(internal, anyelement, csv_options)
  returns internal
  language c
  as 'pg_csv';

create aggregate csv_agg(anyelement, csv_options) (
  sfunc     = csv_agg_transfn,
  stype     = internal,
  finalfunc = csv_agg_finalfn,
  parallel  = safe
);

