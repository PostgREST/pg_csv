\set lim random(1000, 2000)

with pgrst_source as (
  select * from orders_customers limit :lim
)
select
    (select coalesce(string_agg(a.k, ','), '')  from (select json_object_keys(r)::text as k from (select row_to_json(hh) as r from pgrst_source as hh limit 1) _) a) ||
    e'\n' ||
    coalesce(string_agg(substring(_postgrest_t::text, 2, length(_postgrest_t::text) - 2), e'\n'), '') as body
from ( select * from pgrst_source limit :lim) _postgrest_t;
