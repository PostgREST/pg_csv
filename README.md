# pg_csv

![PostgreSQL version](https://img.shields.io/badge/postgresql-12+-blue.svg)
[![Coverage Status](https://coveralls.io/repos/github/PostgREST/pg_csv/badge.svg)](https://coveralls.io/github/PostgREST/pg_csv)
[![Tests](https://github.com/PostgREST/pg_csv/actions/workflows/ci.yaml/badge.svg)](https://github.com/PostgREST/pg_csv/actions)

Postgres has the [COPY .. CSV](https://www.postgresql.org/docs/current/sql-copy.html) command, but `COPY` has problems:

- It uses a special protocol, so it doesn't work with other standard features like [prepared statements](https://www.postgresql.org/docs/current/sql-prepare.html), [pipeline mode](https://www.postgresql.org/docs/current/libpq-pipeline-mode.html#LIBPQ-PIPELINE-USING) or [pgbench](https://www.postgresql.org/docs/current/pgbench.html).
- Is not composable. You can't use COPY inside CTEs, subqueries, view definitions or as function arguments.

`pg_csv` offers flexible CSV processing as a solution to these problems.

- Includes a CSV aggregate that composes with SQL expressions.
- Native C extension, almost 2 times faster than SQL queries that try to achieve CSV output (see our [CI results](https://github.com/PostgREST/pg_csv/actions/runs/17367407744)).
- Simple installation, no dependencies except Postgres.

## Installation

Clone this repo and run:

```bash
make && make install
```

To install the extension:

```psql
create extension pg_csv;
```

## csv_agg

Aggregate that builds a CSV respecting [RFC 4180](https://www.ietf.org/rfc/rfc4180.txt), quoting as required.

```sql
create table projects as
select *
from (
  values
    (1, 'Death Star OS', 1),
    (2, 'Windows 95 Rebooted', 1),
    (3, 'Project "Comma,Please"', 2),
    (4, 'Escape ""Plan""', 2),
    (NULL, 'NULL & Void', NULL)
) as _(id, name, client_id);
```

```sql
select csv_agg(x) from projects x;
            csv_agg
--------------------------------
 id,name,client_id             +
 1,Death Star OS,1             +
 2,Windows 95 Rebooted,1       +
 3,"Project ""Comma,Please""",2+
 4,"Escape """"Plan""""",2     +
 ,NULL & Void,
(1 row)
```

### Custom Delimiter

Custom delimiters can be used to produce different formats like pipe-separated values, tab-separated values or semicolon-separated values.

```sql
select csv_agg(x, csv_options(delimiter := '|')) from projects x;
           csv_agg
-----------------------------
 id|name|client_id          +
 1|Death Star OS|1          +
 2|Windows 95 Rebooted|1    +
 3|Open Source Lightsabers|2+
 4|Galactic Payroll System|2+
 7|Bugzilla Revival|3
(1 row)

select csv_agg(x, csv_options(delimiter := E'\t')) from projects x;
              csv_agg
-----------------------------------
 id      name    client_id        +
 1       Death Star OS   1        +
 2       Windows 95 Rebooted     1+
 3       Open Source Lightsabers 2+
 4       Galactic Payroll System 2+
 7       Bugzilla Revival        3
(1 row)
```

> [!NOTE]
> - Newline, carriage return and double quotes are not supported as delimiters to maintain the integrity of the separated values format.
> - The delimiter can only be a single char, if a longer string is specified only the first char will be used.
> - Why use a `csv_options` constructor function instead of extra arguments? Aggregates don't support named arguments in postgres, see a discussion on https://github.com/PostgREST/pg_csv/pull/2#issuecomment-3155740589.

### BOM

You can include a byte-order mark (BOM) to make the CSV compatible with Excel.

```sql
select csv_agg(x, csv_options(bom := true)) from projects x;

      csv_agg
-------------------
﻿id,name,client_id+
 1,Death Star OS,1
 2,Windows 95 Rebooted,1
 3,Open Source Lightsabers,2
 4,Galactic Payroll System,2
 5,Bugzilla Revival,3
(1 row)
```

### Header

You can omit or include the CSV header.

```sql
select csv_agg(x, csv_options(header := false)) from projects x;

           csv_agg
-----------------------------
 1,Death Star OS,1          +
 2,Windows 95 Rebooted,1    +
 3,Open Source Lightsabers,2+
 4,Galactic Payroll System,2+
 7,Bugzilla Revival,3
(1 row)
```

### Null string

NULL values are represented by an empty string by default. This can be changed with the `nullstr` option.

```sql
SELECT csv_agg(x, csv_options(nullstr:='<NULL>')) AS body
FROM   projects x;

              body
--------------------------------
 id,name,client_id             +
 1,Death Star OS,1             +
 2,Windows 95 Rebooted,1       +
 3,"Project ""Comma,Please""",2+
 4,"Escape """"Plan""""",2     +
 <NULL>,NULL & Void,<NULL>
(1 row)
```

## Limitations

- For large bulk exports and imports, `COPY ... CSV` should still be preferred as its faster due to streaming support.
