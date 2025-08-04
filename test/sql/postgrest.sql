-- recreate postgrest problem in https://github.com/PostgREST/postgrest/issues/3627
create function getproject(id int) returns setof projects
    language sql
    as $_$
    select * from projects where id = $1;
$_$;
\echo

-- this is postgREST CSV query, it produces the same result as our `csv_agg` (see ../expected/sanity.out)
WITH pgrst_source AS (
    SELECT "projects".* FROM "projects"
)
SELECT
    (SELECT coalesce(string_agg(a.k, ','), '')  FROM (SELECT json_object_keys(r)::text as k FROM (SELECT row_to_json(hh) as r from pgrst_source as hh limit 1) _) a) ||
    E'\n' ||
    coalesce(string_agg(substring(_postgrest_t::text, 2, length(_postgrest_t::text) - 2), E'\n'), '') AS body
FROM ( SELECT * FROM pgrst_source ) _postgrest_t;

-- postgREST CSV query with RPC and a filter, it selects columns in a particular order (client_id, id, name)
-- and it produces out of order CSV header names
WITH pgrst_source AS (
    SELECT "pgrst_call"."client_id", "pgrst_call"."id", "pgrst_call"."name" FROM "getproject"("id" := 2) pgrst_call
)
SELECT
    (SELECT coalesce(string_agg(a.k, ','), '')  FROM (    SELECT json_object_keys(r)::text as k  FROM (SELECT row_to_json(hh) as r from pgrst_source as hh limit 1    ) s  ) a) ||
    E'\n' ||
    coalesce(string_agg(substring(_postgrest_t::text, 2, length(_postgrest_t::text) - 2), E'\n'), '') AS body
FROM (SELECT "projects"."name", "projects"."id", "projects"."client_id" FROM "pgrst_source" AS "projects"   ) _postgrest_t;

-- same as above but with our csv_agg query with RPC and filter, it selects columns in a particular order (client_id, id, name)
-- and it now produces correct order of CSV header names
WITH pgrst_source AS (
    SELECT "pgrst_call"."client_id", "pgrst_call"."id", "pgrst_call"."name" FROM "getproject"("id" := 2) pgrst_call
)
SELECT csv_agg(_postgrest_t) AS body
FROM (SELECT "projects"."name", "projects"."id", "projects"."client_id" FROM "pgrst_source" AS "projects") _postgrest_t;
