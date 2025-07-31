create function csv_agg_transfn(internal, anyelement)
  returns internal
  language c
  as 'pg_csv';

create function csv_agg_finalfn(internal)
  returns text
  language c
  as 'pg_csv';

create aggregate csv_agg(anyelement) (
  sfunc      = csv_agg_transfn,
  stype      = internal,
  finalfunc  = csv_agg_finalfn
);
