-- based on the northwind database https://github.com/pthom/northwind_psql
-- the idea is to use the aggregate over a relation with lots of columns to test the performance

create extension if not exists pg_csv;

CREATE TABLE customers (
  customer_id   CHAR(5) PRIMARY KEY,
  company_name  TEXT      NOT NULL,
  contact_name  TEXT,
  contact_title TEXT,
  address       TEXT,
  city          TEXT,
  region        TEXT,
  postal_code   TEXT,
  country       TEXT,
  phone         TEXT,
  fax           TEXT
);

CREATE TABLE orders (
  order_id         BIGSERIAL PRIMARY KEY,
  customer_id      CHAR(5) NOT NULL REFERENCES customers(customer_id) ON DELETE CASCADE,
  employee_id      SMALLINT,
  order_date       DATE,
  required_date    DATE,
  shipped_date     DATE,
  freight          NUMERIC(10,2) DEFAULT 0 CHECK (freight >= 0),
  ship_name        TEXT,
  ship_address     TEXT,
  ship_city        TEXT,
  ship_region      TEXT,
  ship_postal_code TEXT,
  ship_country     TEXT
);

-- generate seed data
-- three groups of 100 by city/country
INSERT INTO customers (
  customer_id, company_name, contact_name, contact_title,
  address, city, region, postal_code, country, phone, fax
)
SELECT
  ('C' || lpad(i::text, 4, '0'))::char(5)                                    AS customer_id,
  'Company ' || i                                                            AS company_name,
  'Contact ' || i                                                            AS contact_name,
  CASE
    WHEN i <= 100 THEN 'Owner'
    WHEN i <= 200 THEN 'Sales Manager'
    ELSE 'Purchasing'
  END                                                                        AS contact_title,
  i::text || ' Main Street'                                                  AS address,
  CASE
    WHEN i <= 100 THEN 'Seattle'
    WHEN i <= 200 THEN 'London'
    ELSE 'Sao Paulo'
  END                                                                        AS city,
  CASE
    WHEN i <= 100 THEN 'WA'
    WHEN i <= 200 THEN NULL
    ELSE 'SP'
  END                                                                        AS region,
  (10000 + i)::text                                                          AS postal_code,
  CASE
    WHEN i <= 100 THEN 'USA'
    WHEN i <= 200 THEN 'UK'
    ELSE 'Brazil'
  END                                                                        AS country,
  '+1-555-' || lpad(i::text, 4, '0')                                         AS phone,
  CASE
    WHEN right(i::text, 1) IN ('0','5') THEN NULL
    ELSE '+1-555-' || lpad((i + 1000)::text, 4, '0')
  END                                                                        AS fax
FROM generate_series(1, 300) AS s(i);

-- 2700 orders, 9 orders per customer
WITH base AS (
  SELECT c.customer_id, c.company_name, c.address, c.city, c.region, c.postal_code, c.country
  FROM customers c
)
INSERT INTO orders (
  customer_id, employee_id, order_date, required_date, shipped_date,
  freight, ship_name, ship_address, ship_city, ship_region, ship_postal_code, ship_country
)
SELECT
  b.customer_id,
  n::smallint                                                        AS employee_id,
  (DATE '2024-01-01' + (n || ' day')::interval)::date               AS order_date,
  (DATE '2024-01-01' + ((n + 7) || ' day')::interval)::date         AS required_date,
  CASE WHEN n = 9 THEN NULL
       ELSE (DATE '2024-01-01' + ((n + 3) || ' day')::interval)::date
  END                                                               AS shipped_date,
  (10 + n)::numeric(10,2)                                           AS freight,
  b.company_name                                                    AS ship_name,
  b.address                                                         AS ship_address,
  b.city                                                            AS ship_city,
  b.region                                                          AS ship_region,
  b.postal_code                                                     AS ship_postal_code,
  b.country                                                         AS ship_country
FROM base b
CROSS JOIN generate_series(1, 9) AS n;

-- create a view to have more columns
CREATE OR REPLACE VIEW orders_customers AS
SELECT
  o.order_id,
  o.customer_id,
  c.company_name,
  c.contact_name,
  c.contact_title,
  c.address          AS customer_address,
  c.city             AS customer_city,
  c.region           AS customer_region,
  c.postal_code      AS customer_postal_code,
  c.country          AS customer_country,
  c.phone,
  c.fax,
  o.employee_id,
  o.order_date,
  o.required_date,
  o.shipped_date,
  o.freight,
  o.ship_name,
  o.ship_address,
  o.ship_city,
  o.ship_region,
  o.ship_postal_code,
  o.ship_country
FROM orders o
JOIN customers c USING (customer_id);
