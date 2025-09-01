\set lim random(1000, 2000)

select csv_agg(t, csv_options(delimiter:='|', bom:=true, header:=false, nullstr:='<NULL>')) from (
  select * from orders_customers limit :lim
) as t;
