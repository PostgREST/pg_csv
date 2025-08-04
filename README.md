# pg_csv

![PostgreSQL version](https://img.shields.io/badge/postgresql-12+-blue.svg)
[![Coverage Status](https://coveralls.io/repos/github/PostgREST/pg_csv/badge.svg)](https://coveralls.io/github/PostgREST/pg_csv)
[![Tests](https://github.com/PostgREST/pg_csv/actions/workflows/ci.yaml/badge.svg)](https://github.com/PostgREST/pg_csv/actions)

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

Aggregate that builds a CSV as per [RFC 4180](https://www.ietf.org/rfc/rfc4180.txt), quoting as required.

```psql
select csv_agg(x) from projects x;
      csv_agg
-------------------
 id,name,client_id+
 1,Windows 7,1    +
 2,Windows 10,1   +
 3,IOS,2          +
 4,OSX,2          +
 5,Orphan,
(1 row)
```

It also supports adding a custom delimiter.

```psql
select csv_agg(x, '|') from projects x;
      csv_agg
-------------------
 id|name|client_id+
 1|Windows 7|1    +
 2|Windows 10|1   +
 3|IOS|2          +
 4|OSX|2          +
 5|Orphan|
(1 row)
```

> [!IMPORTANT]
> Newline, carriage return and double quotes are not supported as delimiters to maintain the integrity of the separated values format.
