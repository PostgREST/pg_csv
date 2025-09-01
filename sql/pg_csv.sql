create type csv_options as
( delimiter "char"
, bom       bool
, header    bool
, nullstr   text
);

create or replace function csv_options
( delimiter "char" default NULL
, bom       bool default NULL
, header    bool default NULL
, nullstr   text default NULL
) returns csv_options as $$
  select row(delimiter, bom, header, nullstr)::csv_options;
$$ language sql;

create function csv_agg_transfn(internal, anyelement)
  returns internal
  language c
as 'MODULE_PATHNAME';

create function csv_agg_transfn(internal, anyelement, csv_options)
  returns internal
  language c
as 'MODULE_PATHNAME';

create function csv_agg_finalfn(internal)
  returns text
  language c
as 'MODULE_PATHNAME';

create aggregate csv_agg(anyelement) (
  sfunc     = csv_agg_transfn,
  stype     = internal,
  finalfunc = csv_agg_finalfn,
  parallel  = safe
);

create aggregate csv_agg(anyelement, csv_options) (
  sfunc     = csv_agg_transfn,
  stype     = internal,
  finalfunc = csv_agg_finalfn,
  parallel  = safe
);
