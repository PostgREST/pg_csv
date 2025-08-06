SELECT csv_agg(x) AS body
FROM   projects x;

-- proves that https://github.com/PostgREST/postgrest/issues/1371#issuecomment-519248984 is solved
select csv_agg(x) from nasty x;
