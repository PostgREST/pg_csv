\set lim random(1000, 2000)

select csv_agg(t) from (
  select * from orders_customers limit :lim
) as t;
