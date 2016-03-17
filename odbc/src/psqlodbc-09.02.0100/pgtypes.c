/*--------
 * Module:			pgtypes.c
 *
 * Description:		This module contains routines for getting information
 *					about the supported Postgres data types.  Only the
 *					function pgtype_to_sqltype() returns an unknown condition.
 *					All other functions return a suitable default so that
 *					even data types that are not directly supported can be
 *					used (it is handled as char data).
 *
 * Classes:			n/a
 *
 * API functions:	none
 *
 * Comments:		See "readme.txt" for copyright and license information.
 *--------
 */

#include "pgtypes.h"

#include "dlg_specific.h"
#include "statement.h"
#include "connection.h"
#include "environ.h"
#include "qresult.h"

#define	EXPERIMENTAL_CURRENTLY


Int4		getCharColumnSize(const StatementClass *stmt, OID type, int col, int handle_unknown_size_as);

/*
 * these are the types we support.	all of the pgtype_ functions should
 * return values for each one of these.
 * Even types not directly supported are handled as character types
 * so all types should work (points, etc.)
 */

/*
 * ALL THESE TYPES ARE NO LONGER REPORTED in SQLGetTypeInfo.  Instead, all
 *	the SQL TYPES are reported and mapped to a corresponding Postgres Type
 */

/*
OID pgtypes_defined[][2] = {
			{PG_TYPE_CHAR, 0}
			,{PG_TYPE_CHAR2, 0}
			,{PG_TYPE_CHAR4, 0}
			,{PG_TYPE_CHAR8, 0}
			,{PG_TYPE_CHAR16, 0}
			,{PG_TYPE_NAME, 0}
			,{PG_TYPE_VARCHAR, 0}
			,{PG_TYPE_BPCHAR, 0}
			,{PG_TYPE_DATE, 0}
			,{PG_TYPE_TIME, 0}
			,{PG_TYPE_TIME_WITH_TMZONE, 0}
			,{PG_TYPE_DATETIME, 0}
			,{PG_TYPE_ABSTIME, 0}
			,{PG_TYPE_TIMESTAMP_NO_TMZONE, 0}
			,{PG_TYPE_TIMESTAMP, 0}
			,{PG_TYPE_TEXT, 0}
			,{PG_TYPE_INT2, 0}
			,{PG_TYPE_INT4, 0}
			,{PG_TYPE_FLOAT4, 0}
			,{PG_TYPE_FLOAT8, 0}
			,{PG_TYPE_OID, 0}
			,{PG_TYPE_MONEY, 0}
			,{PG_TYPE_BOOL, 0}
			,{PG_TYPE_BYTEA, 0}
			,{PG_TYPE_NUMERIC, 0}
			,{PG_TYPE_XID, 0}
			,{PG_TYPE_LO_UNDEFINED, 0}
			,{0, 0} };
*/


/*	These are NOW the SQL Types reported in SQLGetTypeInfo.  */
SQLSMALLINT	sqlTypes[] = {
	SQL_BIGINT,
	/* SQL_BINARY, -- Commented out because VarBinary is more correct. */
	SQL_BIT,
	SQL_CHAR,
#if (ODBCVER >= 0x0300)
	SQL_TYPE_DATE,
#endif /* ODBCVER */
	SQL_DATE,
	SQL_DECIMAL,
	SQL_DOUBLE,
	SQL_FLOAT,
	SQL_INTEGER,
	SQL_LONGVARBINARY,
	SQL_LONGVARCHAR,
	SQL_NUMERIC,
	SQL_REAL,
	SQL_SMALLINT,
#if (ODBCVER >= 0x0300)
	SQL_TYPE_TIME,
	SQL_TYPE_TIMESTAMP,
#endif /* ODBCVER */
	SQL_TIME,
	SQL_TIMESTAMP,
	SQL_TINYINT,
	SQL_VARBINARY,
	SQL_VARCHAR,
#ifdef	UNICODE_SUPPORT
	SQL_WCHAR,
	SQL_WVARCHAR,
	SQL_WLONGVARCHAR,
#endif /* UNICODE_SUPPORT */
#if (ODBCVER >= 0x0350)
	SQL_GUID,
#endif /* ODBCVER */
/* AFAIK SQL_INTERVAL types cause troubles in some spplications */ 
#ifdef	PG_INTERVAL_AS_SQL_INTERVAL
	SQL_INTERVAL_MONTH,
	SQL_INTERVAL_YEAR,
	SQL_INTERVAL_YEAR_TO_MONTH,
	SQL_INTERVAL_DAY,
	SQL_INTERVAL_HOUR,
	SQL_INTERVAL_MINUTE,
	SQL_INTERVAL_SECOND,
	SQL_INTERVAL_DAY_TO_HOUR,
	SQL_INTERVAL_DAY_TO_MINUTE,
	SQL_INTERVAL_DAY_TO_SECOND,
	SQL_INTERVAL_HOUR_TO_MINUTE,
	SQL_INTERVAL_HOUR_TO_SECOND,
	SQL_INTERVAL_MINUTE_TO_SECOND,
#endif /* PG_INTERVAL_AS_SQL_INTERVAL */
	0
};

#if (ODBCVER >= 0x0300) && defined(ODBCINT64)
#define	ALLOWED_C_BIGINT	SQL_C_SBIGINT
/* #define	ALLOWED_C_BIGINT	SQL_C_CHAR */ /* Delphi should be either ? */
#else
#define	ALLOWED_C_BIGINT	SQL_C_CHAR
#endif

OID
pg_true_type(const ConnectionClass *conn, OID type, OID basetype)
{
	if (0 == basetype)
		return type;
	else if (0 == type)
		return basetype;
	else if (type == conn->lobj_type)
		return type;
	return basetype;
}

#define MONTH_BIT 	(1 << 17)
#define YEAR_BIT	(1 << 18)
#define DAY_BIT		(1 << 19)
#define HOUR_BIT	(1 << 26)
#define MINUTE_BIT	(1 << 27)
#define SECOND_BIT	(1 << 28)

#if	(ODBCVER >= 0x0300)
static SQLSMALLINT
get_interval_type(Int4 atttypmod, const char **name)
{
mylog("!!! %s atttypmod=%x\n", __FUNCTION__, atttypmod);
	if ((-1) == atttypmod)
		return 0;
	if (0 != (YEAR_BIT & atttypmod))
	{
		if (0 != (MONTH_BIT & atttypmod))
		{
			if (name)
				*name = "interval year to month";
			return SQL_INTERVAL_YEAR_TO_MONTH;
		}
		if (name)
			*name = "interval year";
		return SQL_INTERVAL_YEAR;
	}
	else if (0 != (MONTH_BIT & atttypmod))
	{
		if (name)
			*name = "interval month";
		return SQL_INTERVAL_MONTH;
	}
	else if (0 != (DAY_BIT & atttypmod))
	{
		if (0 != (SECOND_BIT & atttypmod))
		{
			if (name)
				*name = "interval day to second";
			return SQL_INTERVAL_DAY_TO_SECOND;
		}
		else if (0 != (MINUTE_BIT & atttypmod))
		{
			if (name)
				*name = "interval day to minute";
			return SQL_INTERVAL_DAY_TO_MINUTE;
		}
		else if (0 != (HOUR_BIT & atttypmod))
		{
			if (name)
				*name = "interval day to hour";
			return SQL_INTERVAL_DAY_TO_HOUR;
		}
		if (name)
			*name = "interval day";
		return SQL_INTERVAL_DAY;
	}
	else if (0 != (HOUR_BIT & atttypmod))
	{
		if (0 != (SECOND_BIT & atttypmod))
		{
			if (name)
				*name = "interval hour to second";
			return SQL_INTERVAL_HOUR_TO_SECOND;
		}
		else if (0 != (MINUTE_BIT & atttypmod))
		{
			if (name)
				*name = "interval hour to minute";
			return SQL_INTERVAL_HOUR_TO_MINUTE;
		}
		if (name)
			*name = "interval hour";
		return SQL_INTERVAL_HOUR;
	}
	else if (0 != (MINUTE_BIT & atttypmod))
	{
		if (0 != (SECOND_BIT & atttypmod))
		{
			if (name)
				*name = "interval minute to second";
			return SQL_INTERVAL_MINUTE_TO_SECOND;
		}
		if (name)
			*name = "interval minute";
		return SQL_INTERVAL_MINUTE;
	}
	else if (0 != (SECOND_BIT & atttypmod))
	{
		if (name)
			*name = "interval second";
		return SQL_INTERVAL_SECOND;
	}

	if (name)
		*name = "interval";
	return 0;
}
#endif /* ODBCVER */

static Int4
getCharColumnSizeX(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longestlen, int handle_unknown_size_as)
{
	int		p = -1, maxsize;
	const ConnInfo	*ci = &(conn->connInfo);

	mylog("%s: type=%d, atttypmod=%d, adtsize_or=%d, unknown = %d\n", __FUNCTION__, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as);

	/* Assign Maximum size based on parameters */
	switch (type)
	{
		case PG_TYPE_TEXT:
			if (ci->drivers.text_as_longvarchar)
				maxsize = ci->drivers.max_longvarchar_size;
			else
				maxsize = ci->drivers.max_varchar_size;
			break;

		case PG_TYPE_VARCHAR:
		case PG_TYPE_BPCHAR:
			maxsize = ci->drivers.max_varchar_size;
			break;

		default:
			if (ci->drivers.unknowns_as_longvarchar)
				maxsize = ci->drivers.max_longvarchar_size;
			else
				maxsize = ci->drivers.max_varchar_size;
			break;
	}
#ifdef	UNICODE_SUPPORT
	if (CC_is_in_unicode_driver(conn) &&
	    isSqlServr() &&
	    maxsize > 4000)
		maxsize = 4000;
#endif /* UNICODE_SUPPORT */

	if (maxsize == TEXT_FIELD_SIZE + 1) /* magic length for testing */
	{
		if (PG_VERSION_GE(conn, 7.1))
			maxsize = 0;
		else
			maxsize = TEXT_FIELD_SIZE;
	}
	/*
	 * Static ColumnSize (i.e., the Maximum ColumnSize of the datatype) This
	 * has nothing to do with a result set.
	 */
inolog("!!! atttypmod  < 0 ?\n");
	if (atttypmod < 0 && adtsize_or_longestlen < 0)
		return maxsize;

	/*
	 * Catalog Result Sets -- use assigned column width (i.e., from
	 * set_tuplefield_string)
	 */
inolog("!!! catalog_result=%d\n", handle_unknown_size_as);
	if (UNKNOWNS_AS_CATALOG == handle_unknown_size_as)
	{
		if (adtsize_or_longestlen > 0)
			return adtsize_or_longestlen;
		return maxsize;
	}
	if (TYPE_MAY_BE_ARRAY(type))
	{
		if (adtsize_or_longestlen > 0)
			return adtsize_or_longestlen;
		return maxsize;
	}

inolog("!!! adtsize_or_logngest=%d\n", adtsize_or_longestlen);
	p = adtsize_or_longestlen; /* longest */
	/* Size is unknown -- handle according to parameter */
	if (atttypmod > 0)	/* maybe the length is known */
	{
		if (atttypmod >= p)
			return atttypmod;
		switch (type)
		{
			case PG_TYPE_VARCHAR:
			case PG_TYPE_BPCHAR:
#if (ODBCVER >= 0x0300)
				return atttypmod;
#else
				if (CC_is_in_unicode_driver(conn) || conn->ms_jet)
					return atttypmod;
				return p;
#endif /* ODBCVER */
		}
	}

	/* The type is really unknown */
	switch (handle_unknown_size_as)
	{
		case UNKNOWNS_AS_DONTKNOW:
			return -1;
		case UNKNOWNS_AS_LONGEST:
			mylog("%s: LONGEST: p = %d\n", __FUNCTION__, p);
			if (p > 0)
				return p;
			break;
		case UNKNOWNS_AS_MAX:
			break;
		default:
			return -1;
	}
	if (maxsize <= 0)
		return maxsize;
	switch (type)
	{
		case PG_TYPE_BPCHAR:
		case PG_TYPE_VARCHAR:
		case PG_TYPE_TEXT:
			return maxsize;
	}

	if (p > maxsize)
		maxsize = p;
	return maxsize;
}

static SQLSMALLINT
getNumericDecimalDigitsX(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longest, int handle_unknown_size_as)
{
	Int4		default_decimal_digits = 6;

	mylog("%s: type=%d, atttypmod=%d\n", __FUNCTION__, type, atttypmod);

	if (atttypmod < 0 && adtsize_or_longest < 0)
		return default_decimal_digits;

	if (atttypmod > -1)
		return (atttypmod & 0xffff);
	if (adtsize_or_longest <= 0)
		return default_decimal_digits;
	adtsize_or_longest >>= 16; /* extract the scale part */
	return adtsize_or_longest;
}

static Int4	/* PostgreSQL restritiction */
getNumericColumnSizeX(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longest, int handle_unknown_size_as)
{
	Int4	default_column_size = 28;

	mylog("%s: type=%d, typmod=%d\n", __FUNCTION__, type, atttypmod);

	if (atttypmod > -1)
		return (atttypmod >> 16) & 0xffff;
	switch (handle_unknown_size_as)
	{
		case UNKNOWNS_AS_DONTKNOW:
			return SQL_NO_TOTAL;
	}
	if (adtsize_or_longest <= 0)
		return default_column_size;
	adtsize_or_longest %= (1 << 16); /* extract the precision part */
	switch (handle_unknown_size_as)
	{
		case UNKNOWNS_AS_MAX:
			return adtsize_or_longest > default_column_size ? adtsize_or_longest : default_column_size;
		case UNKNOWNS_AS_CATALOG:
			break;
		default:
			if (adtsize_or_longest < 10)
				adtsize_or_longest = 10;
	}
	return adtsize_or_longest;
}

static SQLSMALLINT
getTimestampDecimalDigitsX(const ConnectionClass *conn, OID type, int atttypmod)
{
	mylog("%s: type=%d, atttypmod=%d\n", __FUNCTION__, type, atttypmod);

	if (PG_VERSION_LT(conn, 7.2))
		return 0;

	return (atttypmod > -1 ? atttypmod : 6);
}


static SQLSMALLINT
getTimestampColumnSizeX(const ConnectionClass *conn, OID type, int atttypmod)
{
	Int4	fixed, scale;

	mylog("%s: type=%d, atttypmod=%d\n", __FUNCTION__, type, atttypmod);

	switch (type)
	{
		case PG_TYPE_TIME:
			fixed = 8;
			break;
		case PG_TYPE_TIME_WITH_TMZONE:
			fixed = 11;
			break;
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
			fixed = 19;
			break;
		default:
			if (USE_ZONE)
				fixed = 22;
			else
				fixed = 19;
			break;
	}
	scale = getTimestampDecimalDigitsX(conn, type, atttypmod);
	return (scale > 0) ? fixed + 1 + scale : fixed;
}

static SQLSMALLINT
getIntervalDecimalDigits(OID type, int atttypmod)
{
	Int4	prec;

	mylog("%s: type=%d, atttypmod=%d\n", __FUNCTION__, type, atttypmod);

	if ((atttypmod & SECOND_BIT) == 0)
		return 0;
	return (prec = atttypmod & 0xffff) == 0xffff ? 6 : prec ;
}

static SQLSMALLINT
getIntervalColumnSize(OID type, int atttypmod)
{
	Int4	ttl, leading_precision = 9, scale;

	mylog("%s: type=%d, atttypmod=%d\n", __FUNCTION__, type, atttypmod);

	ttl = leading_precision;
#if (ODBCVER >= 0x0300)
	switch (get_interval_type(atttypmod, NULL))
	{
		case 0:
			ttl = 25;
			break;
		case SQL_INTERVAL_YEAR:
			ttl = 16;
			break;
		case SQL_INTERVAL_MONTH:
			ttl = 16;
			break;
		case SQL_INTERVAL_DAY:
			ttl = 16;
			break;
		case SQL_INTERVAL_DAY_TO_SECOND:
		case SQL_INTERVAL_DAY_TO_MINUTE:
		case SQL_INTERVAL_DAY_TO_HOUR:
			ttl = 25;
			break;
		case SQL_INTERVAL_HOUR_TO_SECOND:
		case SQL_INTERVAL_HOUR_TO_MINUTE:
		case SQL_INTERVAL_HOUR:
			ttl = 17;
			break;
		case SQL_INTERVAL_MINUTE_TO_SECOND:
		case SQL_INTERVAL_MINUTE:
			ttl = 15;
			break;
		case SQL_INTERVAL_YEAR_TO_MONTH:
			ttl = 24;
			break;
	}
#else
	ttl += 9;
#endif /* ODBCVER */
	scale = getIntervalDecimalDigits(type, atttypmod);
	return (scale > 0) ? ttl + 1 + scale : ttl;
}


SQLSMALLINT
pgtype_attr_to_concise_type(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longestlen)
{
	const ConnInfo	*ci = &(conn->connInfo);
#if (ODBCVER >= 0x0300)
	EnvironmentClass *env = (EnvironmentClass *) CC_get_env(conn);
#ifdef	PG_INTERVAL_AS_SQL_INTERVAL
	SQLSMALLINT	sqltype;
#endif /* PG_INTERVAL_AS_SQL_INTERVAL */
#endif /* ODBCVER */

	switch (type)
	{
		case PG_TYPE_CHAR:
		case PG_TYPE_CHAR2:
		case PG_TYPE_CHAR4:
		case PG_TYPE_CHAR8:
			return ALLOW_WCHAR(conn) ? SQL_WCHAR : SQL_CHAR;
		case PG_TYPE_NAME:
		case PG_TYPE_REFCURSOR:
			return ALLOW_WCHAR(conn) ? SQL_WVARCHAR : SQL_VARCHAR;

#ifdef	UNICODE_SUPPORT
		case PG_TYPE_BPCHAR:
			if (getCharColumnSizeX(conn, type, atttypmod, adtsize_or_longestlen, UNKNOWNS_AS_DEFAULT) > ci->drivers.max_varchar_size)
				return ALLOW_WCHAR(conn) ? SQL_WLONGVARCHAR : SQL_LONGVARCHAR;
			return ALLOW_WCHAR(conn) ? SQL_WCHAR : SQL_CHAR;

		case PG_TYPE_VARCHAR:
			if (getCharColumnSizeX(conn, type, atttypmod, adtsize_or_longestlen, UNKNOWNS_AS_DEFAULT) > ci->drivers.max_varchar_size)
				return ALLOW_WCHAR(conn) ? SQL_WLONGVARCHAR : SQL_LONGVARCHAR;
			return ALLOW_WCHAR(conn) ? SQL_WVARCHAR : SQL_VARCHAR;

		case PG_TYPE_TEXT:
			return ci->drivers.text_as_longvarchar ? 
				(ALLOW_WCHAR(conn) ? SQL_WLONGVARCHAR : SQL_LONGVARCHAR) :
				(ALLOW_WCHAR(conn) ? SQL_WVARCHAR : SQL_VARCHAR);

#else
		case PG_TYPE_BPCHAR:
			if (getCharColumnSizeX(conn, type, atttypmod, adtsize_or_longestlen, UNKNOWNS_AS_DEFAULT) > ci->drivers.max_varchar_size)
				return SQL_LONGVARCHAR;
			return SQL_CHAR;

		case PG_TYPE_VARCHAR:
			if (getCharColumnSizeX(conn, type, atttypmod, adtsize_or_longestlen, UNKNOWNS_AS_DEFAULT) > ci->drivers.max_varchar_size)
				return SQL_LONGVARCHAR;
			return SQL_VARCHAR;

		case PG_TYPE_TEXT:
			return ci->drivers.text_as_longvarchar ? SQL_LONGVARCHAR : SQL_VARCHAR;
#endif /* UNICODE_SUPPORT */

		case PG_TYPE_BYTEA:
			if (ci->bytea_as_longvarbinary)
				return SQL_LONGVARBINARY;
			else
				return SQL_VARBINARY;
		case PG_TYPE_LO_UNDEFINED:
			return SQL_LONGVARBINARY;

		case PG_TYPE_INT2:
			return SQL_SMALLINT;

		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
			return SQL_INTEGER;

			/* Change this to SQL_BIGINT for ODBC v3 bjm 2001-01-23 */
		case PG_TYPE_INT8:
			if (ci->int8_as != 0) 
				return ci->int8_as;
			if (conn->ms_jet) 
				return SQL_NUMERIC; /* maybe a little better than SQL_VARCHAR */
#if (ODBCVER >= 0x0300)
			return SQL_BIGINT;
#else
			return SQL_VARCHAR;
#endif /* ODBCVER */

		case PG_TYPE_NUMERIC:
			return SQL_NUMERIC;

		case PG_TYPE_FLOAT4:
			return SQL_REAL;
		case PG_TYPE_FLOAT8:
			return SQL_FLOAT;
		case PG_TYPE_DATE:
#if (ODBCVER >= 0x0300)
			if (EN_is_odbc3(env))
				return SQL_TYPE_DATE;
#endif /* ODBCVER */
			return SQL_DATE;
		case PG_TYPE_TIME:
#if (ODBCVER >= 0x0300)
			if (EN_is_odbc3(env))
				return SQL_TYPE_TIME;
#endif /* ODBCVER */
			return SQL_TIME;
		case PG_TYPE_ABSTIME:
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
		case PG_TYPE_TIMESTAMP:
#if (ODBCVER >= 0x0300)
			if (EN_is_odbc3(env))
				return SQL_TYPE_TIMESTAMP;
#endif /* ODBCVER */
			return SQL_TIMESTAMP;
		case PG_TYPE_MONEY:
			return SQL_FLOAT;
		case PG_TYPE_BOOL:
			return ci->drivers.bools_as_char ? SQL_VARCHAR : SQL_BIT;
		case PG_TYPE_XML:
			return ALLOW_WCHAR(conn) ? SQL_WLONGVARCHAR : SQL_LONGVARCHAR;
		case PG_TYPE_INET:
		case PG_TYPE_CIDR:
		case PG_TYPE_MACADDR:
			return ALLOW_WCHAR(conn) ? SQL_WVARCHAR : SQL_VARCHAR;
		case PG_TYPE_UUID:
#if (ODBCVER >= 0x0350)
			return SQL_GUID;
#endif /* ODBCVER */
			return ALLOW_WCHAR(conn) ? SQL_WVARCHAR : SQL_VARCHAR;

		case PG_TYPE_INTERVAL:
#ifdef	PG_INTERVAL_AS_SQL_INTERVAL
			if (sqltype = get_interval_type(atttypmod, NULL), 0 != sqltype)
				return sqltype;
#endif /* PG_INTERVAL_AS_SQL_INTERVAL */
			return CC_is_in_unicode_driver(conn) ? SQL_WVARCHAR : SQL_VARCHAR;

		default:

			/*
			 * first, check to see if 'type' is in list.  If not, look up
			 * with query. Add oid, name to list.  If it's already in
			 * list, just return.
			 */
			/* hack until permanent type is available */
			if (type == conn->lobj_type)
				return SQL_LONGVARBINARY;

#ifdef	EXPERIMENTAL_CURRENTLY
			if (ALLOW_WCHAR(conn))
				return ci->drivers.unknowns_as_longvarchar ? SQL_WLONGVARCHAR : SQL_WVARCHAR;
#endif	/* EXPERIMENTAL_CURRENTLY */
			return ci->drivers.unknowns_as_longvarchar ? SQL_LONGVARCHAR : SQL_VARCHAR;
	}
}

SQLSMALLINT
pgtype_attr_to_sqldesctype(const ConnectionClass *conn, OID type, int atttypmod)
{
	SQLSMALLINT	rettype;

#ifdef	PG_INTERVAL_AS_SQL_INTERVAL
	if (PG_TYPE_INTERVAL == type)
		return SQL_INTERVAL;
#endif /* PG_INTERVAL_AS_SQL_INTERVAL */
	switch (rettype = pgtype_attr_to_concise_type(conn, type, atttypmod, PG_UNSPECIFIED))
	{
#if (ODBCVER >= 0x0300)
		case SQL_TYPE_DATE:
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIMESTAMP:
			return SQL_DATETIME;
#endif /* ODBCVER */
	}
	return rettype;
}

SQLSMALLINT
pgtype_attr_to_datetime_sub(const ConnectionClass *conn, OID type, int atttypmod)
{
	SQLSMALLINT	rettype;

	switch (rettype = pgtype_attr_to_concise_type(conn, type, atttypmod, PG_UNSPECIFIED))
	{
#if (ODBCVER >= 0x0300)
		case SQL_TYPE_DATE:
			return SQL_CODE_DATE;
		case SQL_TYPE_TIME:
			return SQL_CODE_TIME;
		case SQL_TYPE_TIMESTAMP:
			return SQL_CODE_TIMESTAMP;
		case SQL_INTERVAL_MONTH:
		case SQL_INTERVAL_YEAR:
		case SQL_INTERVAL_YEAR_TO_MONTH:
		case SQL_INTERVAL_DAY:
		case SQL_INTERVAL_HOUR:
		case SQL_INTERVAL_MINUTE:
		case SQL_INTERVAL_SECOND:
		case SQL_INTERVAL_DAY_TO_HOUR:
		case SQL_INTERVAL_DAY_TO_MINUTE:
		case SQL_INTERVAL_DAY_TO_SECOND:
		case SQL_INTERVAL_HOUR_TO_MINUTE:
		case SQL_INTERVAL_HOUR_TO_SECOND:
		case SQL_INTERVAL_MINUTE_TO_SECOND:
			return rettype - 100;
#endif /* ODBCVER */
	}
	return -1;
}

SQLSMALLINT
pgtype_attr_to_ctype(const ConnectionClass *conn, OID type, int atttypmod)
{
	const ConnInfo	*ci = &(conn->connInfo);
#if (ODBCVER >= 0x0300)
	EnvironmentClass *env = (EnvironmentClass *) CC_get_env(conn);
#ifdef	PG_INTERVAL_AS_SQL_INTERVAL
	SQLSMALLINT	ctype;
#endif /* PG_INTERVAL_A_SQL_INTERVAL */
#endif /* ODBCVER */

	switch (type)
	{
		case PG_TYPE_INT8:
#if (ODBCVER >= 0x0300)
			if (!conn->ms_jet)
				return ALLOWED_C_BIGINT;
#endif /* ODBCVER */
			return SQL_C_CHAR;
		case PG_TYPE_NUMERIC:
			return SQL_C_CHAR;
		case PG_TYPE_INT2:
			return SQL_C_SSHORT;
		case PG_TYPE_OID:
		case PG_TYPE_XID:
			return SQL_C_ULONG;
		case PG_TYPE_INT4:
			return SQL_C_SLONG;
		case PG_TYPE_FLOAT4:
			return SQL_C_FLOAT;
		case PG_TYPE_FLOAT8:
			return SQL_C_DOUBLE;
		case PG_TYPE_DATE:
#if (ODBCVER >= 0x0300)
			if (EN_is_odbc3(env))
				return SQL_C_TYPE_DATE;
#endif /* ODBCVER */
			return SQL_C_DATE;
		case PG_TYPE_TIME:
#if (ODBCVER >= 0x0300)
			if (EN_is_odbc3(env))
				return SQL_C_TYPE_TIME;
#endif /* ODBCVER */
			return SQL_C_TIME;
		case PG_TYPE_ABSTIME:
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
		case PG_TYPE_TIMESTAMP:
#if (ODBCVER >= 0x0300)
			if (EN_is_odbc3(env))
				return SQL_C_TYPE_TIMESTAMP;
#endif /* ODBCVER */
			return SQL_C_TIMESTAMP;
		case PG_TYPE_MONEY:
			return SQL_C_FLOAT;
		case PG_TYPE_BOOL:
			return ci->drivers.bools_as_char ? SQL_C_CHAR : SQL_C_BIT;

		case PG_TYPE_BYTEA:
			return SQL_C_BINARY;
		case PG_TYPE_LO_UNDEFINED:
			return SQL_C_BINARY;
#ifdef	UNICODE_SUPPORT
		case PG_TYPE_BPCHAR:
		case PG_TYPE_VARCHAR:
		case PG_TYPE_TEXT:
			return ALLOW_WCHAR(conn) ? SQL_C_WCHAR : SQL_C_CHAR;
#endif /* UNICODE_SUPPORT */
		case PG_TYPE_UUID:
#if (ODBCVER >= 0x0350)
			if (!conn->ms_jet)
				return SQL_C_GUID;
#endif /* ODBCVER */
			return ALLOW_WCHAR(conn) ? SQL_C_WCHAR : SQL_C_CHAR;

		case PG_TYPE_INTERVAL:
#ifdef	PG_INTERVAL_AS_SQL_INTERVAL
			if (ctype = get_interval_type(atttypmod, NULL), 0 != ctype)
				return ctype;
#endif /* PG_INTERVAL_AS_SQL_INTERVAL */
			return CC_is_in_unicode_driver(conn) ? SQL_C_WCHAR : SQL_CHAR;

		default:
			/* hack until permanent type is available */
			if (type == conn->lobj_type)
				return SQL_C_BINARY;

			/* Experimental, Does this work ? */
#ifdef	EXPERIMENTAL_CURRENTLY
			if (ALLOW_WCHAR(conn))
				return SQL_C_WCHAR;
#endif	/* EXPERIMENTAL_CURRENTLY */
			return SQL_C_CHAR;
	}
}

const char *
pgtype_attr_to_name(const ConnectionClass *conn, OID type, int atttypmod, BOOL auto_increment)
{
	const char	*tname = NULL;

	switch (type)
	{
		case PG_TYPE_CHAR:
			return "char";
		case PG_TYPE_CHAR2:
			return "char2";
		case PG_TYPE_CHAR4:
			return "char4";
		case PG_TYPE_CHAR8:
			return "char8";
		case PG_TYPE_INT8:
			return auto_increment ? "bigserial" : "int8";
		case PG_TYPE_NUMERIC:
			return "numeric";
		case PG_TYPE_VARCHAR:
			return "varchar";
		case PG_TYPE_BPCHAR:
			return "char";
		case PG_TYPE_TEXT:
			return "text";
		case PG_TYPE_NAME:
			return "name";
		case PG_TYPE_REFCURSOR:
			return "refcursor";
		case PG_TYPE_INT2:
			return "int2";
		case PG_TYPE_OID:
			return OID_NAME;
		case PG_TYPE_XID:
			return "xid";
		case PG_TYPE_INT4:
inolog("pgtype_to_name int4\n");
			return auto_increment ? "serial" : "int4";
		case PG_TYPE_FLOAT4:
			return "float4";
		case PG_TYPE_FLOAT8:
			return "float8";
		case PG_TYPE_DATE:
			return "date";
		case PG_TYPE_TIME:
			return "time";
		case PG_TYPE_ABSTIME:
			return "abstime";
		case PG_TYPE_DATETIME:
			if (PG_VERSION_GT(conn, 7.1))
				return "timestamptz";
			else if (PG_VERSION_LT(conn, 7.0))
				return "datetime";
			else
				return "timestamp";
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
			return "timestamp without time zone";
		case PG_TYPE_TIMESTAMP:
			return "timestamp";
		case PG_TYPE_MONEY:
			return "money";
		case PG_TYPE_BOOL:
			return "bool";
		case PG_TYPE_BYTEA:
			return "bytea";
		case PG_TYPE_XML:
			return "xml";
		case PG_TYPE_MACADDR:
			return "macaddr";
		case PG_TYPE_INET:
			return "inet";
		case PG_TYPE_CIDR:
			return "cidr";
		case PG_TYPE_UUID:
			return "uuid";
#if (ODBCVER >= 0x0300)
		case PG_TYPE_INTERVAL:
			get_interval_type(atttypmod, &tname);
			return tname;
#endif /* ODBCVER */

		case PG_TYPE_LO_UNDEFINED:
			return PG_TYPE_LO_NAME;

		default:
			/* hack until permanent type is available */
			if (type == conn->lobj_type)
				return PG_TYPE_LO_NAME;

			/*
			 * "unknown" can actually be used in alter table because it is
			 * a real PG type!
			 */
			return "unknown";
	}
}


Int4	/* PostgreSQL restriction */
pgtype_attr_column_size(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longest, int handle_unknown_size_as)
{
	const ConnInfo	*ci = &(conn->connInfo);

	if (handle_unknown_size_as == UNKNOWNS_AS_DEFAULT)
		handle_unknown_size_as = ci->drivers.unknown_sizes; 
	switch (type)
	{
		case PG_TYPE_CHAR:
			return 1;
		case PG_TYPE_CHAR2:
			return 2;
		case PG_TYPE_CHAR4:
			return 4;
		case PG_TYPE_CHAR8:
			return 8;

		case PG_TYPE_NAME:
		case PG_TYPE_REFCURSOR:
			{
				int	value = 0;
				if (PG_VERSION_GT(conn, 7.4))
				{
					/*
					 * 'conn' argument is marked as const,
					 * because this function just reads
					 * stuff from the already-filled in
					 * fields in the struct. But this is
					 * an exception: CC_get_max_idlen() can
					 * send a SHOW query to the backend to
					 * get the identifier length. Thus cast
					 * away the const here.
					 */
					value = CC_get_max_idlen((ConnectionClass *) conn);
				}
#ifdef	NAME_FIELD_SIZE
				else
					value = NAME_FIELD_SIZE;
#endif /* NAME_FIELD_SIZE */
				if (0 == value)
				{
					if (PG_VERSION_GE(conn, 7.3))
						value = NAMEDATALEN_V73;
					else
						value = NAMEDATALEN_V72;
				}
				return value;
			}

		case PG_TYPE_INT2:
			return 5;

		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
			return 10;

		case PG_TYPE_INT8:
			return 19;			/* signed */

		case PG_TYPE_NUMERIC:
			return getNumericColumnSizeX(conn, type, atttypmod, adtsize_or_longest, handle_unknown_size_as); 

		case PG_TYPE_FLOAT4:
		case PG_TYPE_MONEY:
			return 7;

		case PG_TYPE_FLOAT8:
			return 15;

		case PG_TYPE_DATE:
			return 10;
		case PG_TYPE_TIME:
			return 8;

		case PG_TYPE_ABSTIME:
		case PG_TYPE_TIMESTAMP:
			return 22;
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
			/* return 22; */
			return getTimestampColumnSizeX(conn, type, atttypmod);

		case PG_TYPE_BOOL:
			return ci->drivers.bools_as_char ? PG_WIDTH_OF_BOOLS_AS_CHAR : 1;

		case PG_TYPE_MACADDR:
			return 17;

		case PG_TYPE_INET:
		case PG_TYPE_CIDR:
			return sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255/128");
		case PG_TYPE_UUID:
			return sizeof("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");

		case PG_TYPE_LO_UNDEFINED:
			return SQL_NO_TOTAL;

		case PG_TYPE_INTERVAL:
			return getIntervalColumnSize(type, atttypmod);

		default:

			if (type == conn->lobj_type)	/* hack until permanent
												 * type is available */
				return SQL_NO_TOTAL;
			if (PG_TYPE_BYTEA == type && ci->bytea_as_longvarbinary)
				return SQL_NO_TOTAL;

			/* Handle Character types and unknown types */
			return getCharColumnSizeX(conn, type, atttypmod, adtsize_or_longest, handle_unknown_size_as);
	}
}

SQLSMALLINT
pgtype_attr_precision(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longest, int handle_unknown_size_as)
{
	switch (type)
	{
		case PG_TYPE_NUMERIC:
			return getNumericColumnSizeX(conn, type, atttypmod, adtsize_or_longest, handle_unknown_size_as);
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
			return getTimestampDecimalDigitsX(conn, type, atttypmod);
#ifdef	PG_INTERVAL_AS_SQL_INTERVAL
		case PG_TYPE_INTERVAL:
			return getIntervalDecimalDigits(type, atttypmod);
#endif /* PG_INTERVAL_AS_SQL_INTERVAL */
	}
	return -1;
}

Int4
pgtype_attr_display_size(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longestlen, int handle_unknown_size_as)
{
	int	dsize;

	switch (type)
	{
		case PG_TYPE_INT2:
			return 6;

		case PG_TYPE_OID:
		case PG_TYPE_XID:
			return 10;

		case PG_TYPE_INT4:
			return 11;

		case PG_TYPE_INT8:
			return 20;			/* signed: 19 digits + sign */

		case PG_TYPE_NUMERIC:
			dsize = getNumericColumnSizeX(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as);
			return dsize <= 0 ? dsize : dsize + 2;

		case PG_TYPE_MONEY:
			return 15;			/* ($9,999,999.99) */

		case PG_TYPE_FLOAT4:
			return 13;

		case PG_TYPE_FLOAT8:
			return 22;

		case PG_TYPE_MACADDR:
			return 17;
		case PG_TYPE_INET:
		case PG_TYPE_CIDR:
			return sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255/128");
#if (ODBCVER >= 0x0350)
		case PG_TYPE_UUID:
			return 36;
#endif /* ODBCVER */

		case PG_TYPE_INTERVAL:
			return 30;

			/* Character types use regular precision */
		default:
			return pgtype_attr_column_size(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as);
	}
}

Int4
pgtype_attr_buffer_length(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longestlen, int handle_unknown_size_as)
{
	int	dsize;

	switch (type)
	{
		case PG_TYPE_INT2:
			return 2; /* sizeof(SQLSMALLINT) */

		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
			return 4; /* sizeof(SQLINTEGER) */

		case PG_TYPE_INT8:
			if (SQL_C_CHAR == pgtype_attr_to_ctype(conn, type, atttypmod))
				return 20;			/* signed: 19 digits + sign */
			return 8; /* sizeof(SQLSBININT) */

		case PG_TYPE_NUMERIC:
			dsize = getNumericColumnSizeX(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as);
			return dsize <= 0 ? dsize : dsize + 2;

		case PG_TYPE_FLOAT4:
		case PG_TYPE_MONEY:
			return 4; /* sizeof(SQLREAL) */

		case PG_TYPE_FLOAT8:
			return 8; /* sizeof(SQLFLOAT) */

		case PG_TYPE_DATE:
		case PG_TYPE_TIME:
			return 6;		/* sizeof(DATE(TIME)_STRUCT) */

		case PG_TYPE_ABSTIME:
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
			return 16;		/* sizeof(TIMESTAMP_STRUCT) */

		case PG_TYPE_MACADDR:
			return 17;
		case PG_TYPE_INET:
		case PG_TYPE_CIDR:
			return sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255/128");
		case PG_TYPE_UUID:
#if (ODBCVER >= 0x0350)
			return 16;		/* sizeof(SQLGUID) */
#endif /* ODBCVER */
			return 36;

			/* Character types use the default precision */
		case PG_TYPE_VARCHAR:
		case PG_TYPE_BPCHAR:
			{
			int	coef = 1;
			Int4	prec = pgtype_attr_column_size(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as), maxvarc;
			if (SQL_NO_TOTAL == prec)
				return prec;
#ifdef	UNICODE_SUPPORT
			if (CC_is_in_unicode_driver(conn))
				return prec * WCLEN;
#endif /* UNICODE_SUPPORT */
			/* after 7.2 */
			if (PG_VERSION_GE(conn, 7.2))
				coef = conn->mb_maxbyte_per_char;
			if (coef < 2 && (conn->connInfo).lf_conversion)
				/* CR -> CR/LF */
				coef = 2;
			if (coef == 1)
				return prec;
			maxvarc = conn->connInfo.drivers.max_varchar_size;
			if (prec <= maxvarc && prec * coef > maxvarc)
				return maxvarc;
			return coef * prec;
			}
#ifdef	PG_INTERVAL_AS_SQL_INTERVAL 
		case PG_TYPE_INTERVAL:
			return sizeof(SQL_INTERVAL_STRUCT);
#endif /* PG_INTERVAL_AS_SQL_INTERVAL */ 

		default:
			return pgtype_attr_column_size(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as);
	}
}

/*
 */
Int4
pgtype_attr_desclength(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longestlen, int handle_unknown_size_as)
{
	int	dsize;

	switch (type)
	{
		case PG_TYPE_INT2:
			return 2;

		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
			return 4;

		case PG_TYPE_INT8:
			return 20;			/* signed: 19 digits + sign */

		case PG_TYPE_NUMERIC:
			dsize = getNumericColumnSizeX(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as);
			return dsize <= 0 ? dsize : dsize + 2;

		case PG_TYPE_FLOAT4:
		case PG_TYPE_MONEY:
			return 4;

		case PG_TYPE_FLOAT8:
			return 8;

		case PG_TYPE_DATE:
		case PG_TYPE_TIME:
		case PG_TYPE_ABSTIME:
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
		case PG_TYPE_TIMESTAMP:
		case PG_TYPE_VARCHAR:
		case PG_TYPE_BPCHAR:
			return pgtype_attr_column_size(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as);
		default:
			return pgtype_attr_column_size(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as);
	}
}

Int2
pgtype_attr_decimal_digits(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longestlen, int handle_unknown_size_as)
{
	switch (type)
	{
		case PG_TYPE_INT2:
		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
		case PG_TYPE_INT8:
		case PG_TYPE_FLOAT4:
		case PG_TYPE_FLOAT8:
		case PG_TYPE_MONEY:
		case PG_TYPE_BOOL:

			/*
			 * Number of digits to the right of the decimal point in
			 * "yyyy-mm=dd hh:mm:ss[.f...]"
			 */
		case PG_TYPE_ABSTIME:
		case PG_TYPE_TIMESTAMP:
			return 0;
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
			/* return 0; */
			return getTimestampDecimalDigitsX(conn, type, atttypmod);

		case PG_TYPE_NUMERIC:
			return getNumericDecimalDigitsX(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as);

#ifdef	PG_INTERVAL_AS_SQL_INTERVAL
		case PG_TYPE_INTERVAL:
			return getIntervalDecimalDigits(type, atttypmod);
#endif /* PG_INTERVAL_AS_SQL_INTERVAL */

		default:
			return -1;
	}
}

Int2
pgtype_attr_scale(const ConnectionClass *conn, OID type, int atttypmod, int adtsize_or_longestlen, int handle_unknown_size_as)
{
	switch (type)
	{
		case PG_TYPE_NUMERIC:
			return getNumericDecimalDigitsX(conn, type, atttypmod, adtsize_or_longestlen, handle_unknown_size_as); 
	}
	return -1;
}

Int4
pgtype_attr_transfer_octet_length(const ConnectionClass *conn, OID type, int atttypmod, int handle_unknown_size_as)
{
	int	coef = 1;
	Int4	maxvarc, column_size;
	switch (type)
	{
		case PG_TYPE_VARCHAR:
		case PG_TYPE_BPCHAR:
		case PG_TYPE_TEXT:
			column_size = pgtype_attr_column_size(conn, type, atttypmod, PG_UNSPECIFIED, handle_unknown_size_as);
			if (SQL_NO_TOTAL == column_size)
				return column_size;
#ifdef	UNICODE_SUPPORT
			if (CC_is_in_unicode_driver(conn))
				return column_size * WCLEN;
#endif /* UNICODE_SUPPORT */
			/* after 7.2 */
			if (PG_VERSION_GE(conn, 7.2))
				coef = conn->mb_maxbyte_per_char;
			if (coef < 2 && (conn->connInfo).lf_conversion)
				/* CR -> CR/LF */
				coef = 2;
			if (coef == 1)
				return column_size;
			maxvarc = conn->connInfo.drivers.max_varchar_size;
			if (column_size <= maxvarc && column_size * coef > maxvarc)
				return maxvarc;
			return coef * column_size;
		case PG_TYPE_BYTEA:
			return pgtype_attr_column_size(conn, type, atttypmod, PG_UNSPECIFIED, handle_unknown_size_as);
		default:
			if (type == conn->lobj_type)
				return pgtype_attr_column_size(conn, type, atttypmod, PG_UNSPECIFIED, handle_unknown_size_as);
	}
	return -1;
}


OID
sqltype_to_pgtype(const ConnectionClass *conn, SQLSMALLINT fSqlType)
{
	OID		pgType;
	const ConnInfo	*ci = &(conn->connInfo);

	pgType = 0; /* ??? */
	switch (fSqlType)
	{
		case SQL_BINARY:
			pgType = PG_TYPE_BYTEA;
			break;

		case SQL_CHAR:
			pgType = PG_TYPE_BPCHAR;
			break;

#ifdef	UNICODE_SUPPORT
		case SQL_WCHAR:
			pgType = PG_TYPE_BPCHAR;
			break;
#endif /* UNICODE_SUPPORT */

		case SQL_BIT:
			pgType = ci->drivers.bools_as_char ? PG_TYPE_CHAR : PG_TYPE_BOOL;
			break;

#if (ODBCVER >= 0x0300)
		case SQL_TYPE_DATE:
#endif /* ODBCVER */
		case SQL_DATE:
			pgType = PG_TYPE_DATE;
			break;

		case SQL_DOUBLE:
		case SQL_FLOAT:
			pgType = PG_TYPE_FLOAT8;
			break;

		case SQL_DECIMAL:
		case SQL_NUMERIC:
			pgType = PG_TYPE_NUMERIC;
			break;

		case SQL_BIGINT:
			pgType = PG_TYPE_INT8;
			break;

		case SQL_INTEGER:
			pgType = PG_TYPE_INT4;
			break;

		case SQL_LONGVARBINARY:
			if (ci->bytea_as_longvarbinary)
				pgType = PG_TYPE_BYTEA;
			else
				pgType = conn->lobj_type;
			break;

		case SQL_LONGVARCHAR:
			pgType = ci->drivers.text_as_longvarchar ? PG_TYPE_TEXT : PG_TYPE_VARCHAR;
			break;

#ifdef	UNICODE_SUPPORT
		case SQL_WLONGVARCHAR:
			pgType = ci->drivers.text_as_longvarchar ? PG_TYPE_TEXT : PG_TYPE_VARCHAR;
			break;
#endif /* UNICODE_SUPPORT */

		case SQL_REAL:
			pgType = PG_TYPE_FLOAT4;
			break;

		case SQL_SMALLINT:
		case SQL_TINYINT:
			pgType = PG_TYPE_INT2;
			break;

		case SQL_TIME:
#if (ODBCVER >= 0x0300)
		case SQL_TYPE_TIME:
#endif /* ODBCVER */
			pgType = PG_TYPE_TIME;
			break;

		case SQL_TIMESTAMP:
#if (ODBCVER >= 0x0300)
		case SQL_TYPE_TIMESTAMP:
#endif /* ODBCVER */
			pgType = PG_TYPE_DATETIME;
			break;

		case SQL_VARBINARY:
			pgType = PG_TYPE_BYTEA;
			break;

		case SQL_VARCHAR:
			pgType = PG_TYPE_VARCHAR;
			break;

#if	UNICODE_SUPPORT
		case SQL_WVARCHAR:
			pgType = PG_TYPE_VARCHAR;
			break;
#endif /* UNICODE_SUPPORT */

#if (ODBCVER >= 0x0350)
		case SQL_GUID:
			if (PG_VERSION_GE(conn, 8.3))
				pgType = PG_TYPE_UUID;
			break;
#endif /* ODBCVER */

		case SQL_INTERVAL_MONTH:
		case SQL_INTERVAL_YEAR:
		case SQL_INTERVAL_YEAR_TO_MONTH:
		case SQL_INTERVAL_DAY:
		case SQL_INTERVAL_HOUR:
		case SQL_INTERVAL_MINUTE:
		case SQL_INTERVAL_SECOND:
		case SQL_INTERVAL_DAY_TO_HOUR:
		case SQL_INTERVAL_DAY_TO_MINUTE:
		case SQL_INTERVAL_DAY_TO_SECOND:
		case SQL_INTERVAL_HOUR_TO_MINUTE:
		case SQL_INTERVAL_HOUR_TO_SECOND:
		case SQL_INTERVAL_MINUTE_TO_SECOND:
			pgType = PG_TYPE_INTERVAL;
			break;
	}

	return pgType;
}

static int
getAtttypmodEtc(const StatementClass *stmt, int col, int *adtsize_or_longestlen)
{
	int	atttypmod = -1;

	if (NULL != adtsize_or_longestlen)
		*adtsize_or_longestlen = -1;
	if (col >= 0)
	{
		const QResultClass	*res;

		if (res = SC_get_Curres(stmt), NULL != res)
		{
			atttypmod = QR_get_atttypmod(res, col);
			if (NULL != adtsize_or_longestlen)
			{
				if (stmt->catalog_result)
					*adtsize_or_longestlen = QR_get_fieldsize(res, col);
				else
				{
					*adtsize_or_longestlen = QR_get_display_size(res, col);
					if (PG_TYPE_NUMERIC == QR_get_field_type(res, col) && 
					   atttypmod < 0 &&
					   *adtsize_or_longestlen > 0)
					{
						int i, sval, maxscale = 0;
						const char	*tval, *sptr;

						for (i = 0; i < res->num_cached_rows; i++)
						{
							if (tval = QR_get_value_backend_text(res, i, col), NULL != tval)
							{
								if (sptr = strchr(tval, '.'), NULL != sptr)
									if (sval = strlen(tval) - (sptr + 1 - tval), sval > maxscale)
										maxscale = sval;
							}
						}
						*adtsize_or_longestlen += (maxscale << 16);
					}
				}
			} 
		}
	}
	return atttypmod;
}

/*
 *	There are two ways of calling this function:
 *
 *	1.	When going through the supported PG types (SQLGetTypeInfo)
 *
 *	2.	When taking any type id (SQLColumns, SQLGetData)
 *
 *	The first type will always work because all the types defined are returned here.
 *	The second type will return a default based on global parameter when it does not
 *	know.	This allows for supporting
 *	types that are unknown.  All other pg routines in here return a suitable default.
 */
SQLSMALLINT
pgtype_to_concise_type(const StatementClass *stmt, OID type, int col)
{
	int	atttypmod, adtsize_or_longestlen;

	atttypmod = getAtttypmodEtc(stmt, col, &adtsize_or_longestlen);
	return pgtype_attr_to_concise_type(SC_get_conn(stmt), type, atttypmod, adtsize_or_longestlen); 
}

SQLSMALLINT
pgtype_to_sqldesctype(const StatementClass *stmt, OID type, int col)
{
	int	atttypmod = getAtttypmodEtc(stmt, col, NULL);

	return pgtype_attr_to_sqldesctype(SC_get_conn(stmt), type, atttypmod); 
}

SQLSMALLINT
pgtype_to_datetime_sub(const StatementClass *stmt, OID type, int col)
{
	int	atttypmod = getAtttypmodEtc(stmt, col, NULL);

	return pgtype_attr_to_datetime_sub(SC_get_conn(stmt), type, atttypmod); 
}


SQLSMALLINT
pgtype_to_ctype(const StatementClass *stmt, OID type, int col)
{
	int	atttypmod = getAtttypmodEtc(stmt, col, NULL);

	return pgtype_attr_to_ctype(SC_get_conn(stmt), type, atttypmod); 
}

const char *
pgtype_to_name(const StatementClass *stmt, OID type, int col, BOOL auto_increment)
{
	int	atttypmod = getAtttypmodEtc(stmt, col, NULL);

	return pgtype_attr_to_name(SC_get_conn(stmt), type, atttypmod, auto_increment); 
}


#ifdef	NOT_USED
static SQLSMALLINT
getNumericDecimalDigits(const StatementClass *stmt, OID type, int col)
{
	Int4		atttypmod = -1, default_decimal_digits = 6;
	QResultClass	*result;
	ColumnInfoClass *flds;

	mylog("getNumericDecimalDigits: type=%d, col=%d\n", type, col);

	if (col < 0)
		return default_decimal_digits;

	result = SC_get_Curres(stmt);

	/*
	 * Manual Result Sets -- use assigned column width (i.e., from
	 * set_tuplefield_string)
	 */
	atttypmod = QR_get_atttypmod(result, col);
	if (atttypmod > -1)
		return (atttypmod & 0xffff);
	if (stmt->catalog_result)
	{
		flds = QR_get_fields(result);
		if (flds)
		{
			int	fsize = CI_get_fieldsize(flds, col);
			if (fsize > 0)
				return fsize;
		}
		return default_decimal_digits;
	}
	else
	{
		Int4 dsp_size = QR_get_display_size(result, col);
		if (dsp_size <= 0)
			return default_decimal_digits;
		if (dsp_size < 5)
			dsp_size = 5;
		return dsp_size;
	}
}


static Int4	/* PostgreSQL restritiction */
getNumericColumnSize(const StatementClass *stmt, OID type, int col)
{
	Int4	atttypmod = -1, default_column_size = 28;
	QResultClass *result;
	ColumnInfoClass *flds;

	mylog("getNumericColumnSize: type=%d, col=%d\n", type, col);

	if (col < 0)
		return default_column_size;

	result = SC_get_Curres(stmt);

	/*
	 * Manual Result Sets -- use assigned column width (i.e., from
	 * set_tuplefield_string)
	 */
	atttypmod = QR_get_atttypmod(result, col);
	if (atttypmod > -1)
		return (atttypmod >> 16) & 0xffff;
	if (stmt->catalog_result)
	{
		flds = QR_get_fields(result);
		if (flds)
		{
			int	fsize = CI_get_fieldsize(flds, col);
			if (fsize > 0)
				return 2 * fsize;
		}
		return default_column_size;
	}
	else
	{
		Int4	dsp_size = QR_get_display_size(result, col);
		if (dsp_size <= 0)
			return default_column_size;
		dsp_size *= 2;
		if (dsp_size < 10)
			dsp_size = 10;
		return dsp_size;
	}
}

#ifdef NOT_USED
Int4
getCharColumnSize(const StatementClass *stmt, OID type, int col, int handle_unknown_size_as)
{
	CSTR	func = "getCharColumnSize";
	int		p = -1, attlen = -1, adtsize = -1, maxsize;
	QResultClass	*result;
	ConnectionClass	*conn = SC_get_conn(stmt);
	ConnInfo	*ci = &(conn->connInfo);

	mylog("%s: type=%d, col=%d, unknown = %d\n", func, type, col, handle_unknown_size_as);

	/* Assign Maximum size based on parameters */
	switch (type)
	{
		case PG_TYPE_TEXT:
			if (ci->drivers.text_as_longvarchar)
				maxsize = ci->drivers.max_longvarchar_size;
			else
				maxsize = ci->drivers.max_varchar_size;
			break;

		case PG_TYPE_VARCHAR:
		case PG_TYPE_BPCHAR:
			maxsize = ci->drivers.max_varchar_size;
			break;

		default:
			if (ci->drivers.unknowns_as_longvarchar)
				maxsize = ci->drivers.max_longvarchar_size;
			else
				maxsize = ci->drivers.max_varchar_size;
			break;
	}
#ifdef	UNICODE_SUPPORT
	if (CC_is_in_unicode_driver(conn) &&
	    isSqlServr() &&
	    maxsize > 4000)
		maxsize = 4000;
#endif /* UNICODE_SUPPORT */

	if (maxsize == TEXT_FIELD_SIZE + 1) /* magic length for testing */
	{
		if (PG_VERSION_GE(conn, 7.1))
			maxsize = 0;
		else
			maxsize = TEXT_FIELD_SIZE;
	}
	/*
	 * Static ColumnSize (i.e., the Maximum ColumnSize of the datatype) This
	 * has nothing to do with a result set.
	 */
	if (col < 0)
		return maxsize;

	if (result = SC_get_Curres(stmt), NULL == result)
		return maxsize;

	/*
	 * Catalog Result Sets -- use assigned column width (i.e., from
	 * set_tuplefield_string)
	 */
	adtsize = QR_get_fieldsize(result, col);
	if (stmt->catalog_result)
	{
		if (adtsize > 0)
			return adtsize;
		return maxsize;
	}

	p = QR_get_display_size(result, col); /* longest */
	attlen = QR_get_atttypmod(result, col);
	/* Size is unknown -- handle according to parameter */
	if (attlen > 0)	/* maybe the length is known */
	{
		if (attlen >= p)
			return attlen;
		switch (type)
		{
			case PG_TYPE_VARCHAR:
			case PG_TYPE_BPCHAR:
#if (ODBCVER >= 0x0300)
				return attlen;
#else
				if (CC_is_in_unicode_driver(conn) || conn->ms_jet)
					return attlen;
				return p;
#endif /* ODBCVER */
		}
	}

	/* The type is really unknown */
	switch (handle_unknown_size_as)
	{
		case UNKNOWNS_AS_DONTKNOW:
			return -1;
		case UNKNOWNS_AS_LONGEST:
			mylog("%s: LONGEST: p = %d\n", func, p);
			if (p > 0)
				return p;
			break;
		case UNKNOWNS_AS_MAX:
			break;
		default:
			return -1;
	}
	if (maxsize <= 0)
		return maxsize;
	switch (type)
	{
		case PG_TYPE_BPCHAR:
		case PG_TYPE_VARCHAR:
		case PG_TYPE_TEXT:
			return maxsize;
	}

	if (p > maxsize)
		maxsize = p;
	return maxsize;
}
#endif /* NOT_USED */

static SQLSMALLINT
getTimestampDecimalDigits(const StatementClass *stmt, OID type, int col)
{
	ConnectionClass *conn = SC_get_conn(stmt);
	Int4		atttypmod;
	QResultClass *result;

	mylog("getTimestampDecimalDigits: type=%d, col=%d\n", type, col);

	if (col < 0)
		return 0;
	if (PG_VERSION_LT(conn, 7.2))
		return 0;

	result = SC_get_Curres(stmt);

	atttypmod = QR_get_atttypmod(result, col);
	mylog("atttypmod2=%d\n", atttypmod);
	return (atttypmod > -1 ? atttypmod : 6);
}


static SQLSMALLINT
getTimestampColumnSize(const StatementClass *stmt, OID type, int col)
{
	Int4		fixed,
				scale;

	mylog("getTimestampColumnSize: type=%d, col=%d\n", type, col);

	switch (type)
	{
		case PG_TYPE_TIME:
			fixed = 8;
			break;
		case PG_TYPE_TIME_WITH_TMZONE:
			fixed = 11;
			break;
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
			fixed = 19;
			break;
		default:
			if (USE_ZONE)
				fixed = 22;
			else
				fixed = 19;
			break;
	}
	scale = getTimestampDecimalDigits(stmt, type, col);
	return (scale > 0) ? fixed + 1 + scale : fixed;
}
#endif /* NOT_USED */

/*
 *	This corresponds to "precision" in ODBC 2.x.
 *
 *	For PG_TYPE_VARCHAR, PG_TYPE_BPCHAR, PG_TYPE_NUMERIC, SQLColumns will
 *	override this length with the atttypmod length from pg_attribute .
 *
 *	If col >= 0, then will attempt to get the info from the result set.
 *	This is used for functions SQLDescribeCol and SQLColAttributes.
 */
Int4	/* PostgreSQL restriction */
pgtype_column_size(const StatementClass *stmt, OID type, int col, int handle_unknown_size_as)
{
	int	atttypmod, adtsize_or_longestlen;

	atttypmod = getAtttypmodEtc(stmt, col, &adtsize_or_longestlen);
	return pgtype_attr_column_size(SC_get_conn(stmt), type, atttypmod, adtsize_or_longestlen, stmt->catalog_result ? UNKNOWNS_AS_CATALOG : handle_unknown_size_as); 
}

/*
 *	precision in ODBC 3.x.
 */
SQLSMALLINT
pgtype_precision(const StatementClass *stmt, OID type, int col, int handle_unknown_size_as)
{
	int	atttypmod, adtsize_or_longestlen;

	atttypmod = getAtttypmodEtc(stmt, col, &adtsize_or_longestlen);
	return pgtype_attr_precision(SC_get_conn(stmt), type, atttypmod, adtsize_or_longestlen, stmt->catalog_result ? UNKNOWNS_AS_CATALOG : handle_unknown_size_as); 
}


Int4
pgtype_display_size(const StatementClass *stmt, OID type, int col, int handle_unknown_size_as)
{
	int	atttypmod, adtsize_or_longestlen;

	atttypmod = getAtttypmodEtc(stmt, col, &adtsize_or_longestlen);
	return pgtype_attr_display_size(SC_get_conn(stmt), type, atttypmod, adtsize_or_longestlen, stmt->catalog_result ? UNKNOWNS_AS_CATALOG : handle_unknown_size_as); 
}


/*
 *	The length in bytes of data transferred on an SQLGetData, SQLFetch,
 *	or SQLFetchScroll operation if SQL_C_DEFAULT is specified.
 */
Int4
pgtype_buffer_length(const StatementClass *stmt, OID type, int col, int handle_unknown_size_as)
{
	int	atttypmod, adtsize_or_longestlen;

	atttypmod = getAtttypmodEtc(stmt, col, &adtsize_or_longestlen);
	return pgtype_attr_buffer_length(SC_get_conn(stmt), type, atttypmod, adtsize_or_longestlen, stmt->catalog_result ? UNKNOWNS_AS_CATALOG : handle_unknown_size_as); 
}

/*
 */
Int4
pgtype_desclength(const StatementClass *stmt, OID type, int col, int handle_unknown_size_as)
{
	int	atttypmod, adtsize_or_longestlen;

	atttypmod = getAtttypmodEtc(stmt, col, &adtsize_or_longestlen);
	return pgtype_attr_desclength(SC_get_conn(stmt), type, atttypmod, adtsize_or_longestlen, stmt->catalog_result ? UNKNOWNS_AS_CATALOG : handle_unknown_size_as); 
}

#ifdef	NOT_USED
/*
 *	Transfer octet length.
 */
Int4
pgtype_transfer_octet_length(const StatementClass *stmt, OID type, int column_size)
{
	ConnectionClass	*conn = SC_get_conn(stmt);

	int	coef = 1;
	Int4	maxvarc;
	switch (type)
	{
		case PG_TYPE_VARCHAR:
		case PG_TYPE_BPCHAR:
		case PG_TYPE_TEXT:
			if (SQL_NO_TOTAL == column_size)
				return column_size;
#ifdef	UNICODE_SUPPORT
			if (CC_is_in_unicode_driver(conn))
				return column_size * WCLEN;
#endif /* UNICODE_SUPPORT */
			/* after 7.2 */
			if (PG_VERSION_GE(conn, 7.2))
				coef = conn->mb_maxbyte_per_char;
			if (coef < 2 && (conn->connInfo).lf_conversion)
				/* CR -> CR/LF */
				coef = 2;
			if (coef == 1)
				return column_size;
			maxvarc = conn->connInfo.drivers.max_varchar_size;
			if (column_size <= maxvarc && column_size * coef > maxvarc)
				return maxvarc;
			return coef * column_size;
		case PG_TYPE_BYTEA:
			return column_size;
		default:
			if (type == conn->lobj_type)
				return column_size;
	}
	return -1;
}
#endif /* NOT_USED */

/*
 *	corrsponds to "min_scale" in ODBC 2.x.
 */
Int2
pgtype_min_decimal_digits(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_INT2:
		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
		case PG_TYPE_INT8:
		case PG_TYPE_FLOAT4:
		case PG_TYPE_FLOAT8:
		case PG_TYPE_MONEY:
		case PG_TYPE_BOOL:
		case PG_TYPE_ABSTIME:
		case PG_TYPE_TIMESTAMP:
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
		case PG_TYPE_NUMERIC:
			return 0;
		default:
			return -1;
	}
}

/*
 *	corrsponds to "max_scale" in ODBC 2.x.
 */
Int2
pgtype_max_decimal_digits(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_INT2:
		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
		case PG_TYPE_INT8:
		case PG_TYPE_FLOAT4:
		case PG_TYPE_FLOAT8:
		case PG_TYPE_MONEY:
		case PG_TYPE_BOOL:
		case PG_TYPE_ABSTIME:
		case PG_TYPE_TIMESTAMP:
			return 0;
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
			return 38;
		case PG_TYPE_NUMERIC:
			return getNumericDecimalDigitsX(conn, type, -1, -1, UNKNOWNS_AS_DEFAULT);
		default:
			return -1;
	}
}

/*
 *	corrsponds to "scale" in ODBC 2.x.
 */
Int2
pgtype_decimal_digits(const StatementClass *stmt, OID type, int col)
{
	int	atttypmod, adtsize_or_longestlen;

	atttypmod = getAtttypmodEtc(stmt, col, &adtsize_or_longestlen);
	return pgtype_attr_decimal_digits(SC_get_conn(stmt), type, atttypmod, adtsize_or_longestlen, stmt->catalog_result ? UNKNOWNS_AS_CATALOG : UNKNOWNS_AS_DEFAULT); 
}

/*
 *	"scale" in ODBC 3.x.
 */
Int2
pgtype_scale(const StatementClass *stmt, OID type, int col)
{
	int	atttypmod, adtsize_or_longestlen;

	atttypmod = getAtttypmodEtc(stmt, col, &adtsize_or_longestlen);
	return pgtype_attr_scale(SC_get_conn(stmt), type, atttypmod, adtsize_or_longestlen, stmt->catalog_result ? UNKNOWNS_AS_CATALOG : UNKNOWNS_AS_DEFAULT); 
}


Int2
pgtype_radix(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_INT2:
		case PG_TYPE_XID:
		case PG_TYPE_OID:
		case PG_TYPE_INT4:
		case PG_TYPE_INT8:
		case PG_TYPE_NUMERIC:
		case PG_TYPE_FLOAT4:
		case PG_TYPE_MONEY:
		case PG_TYPE_FLOAT8:
			return 10;
		default:
			return -1;
	}
}


Int2
pgtype_nullable(const ConnectionClass *conn, OID type)
{
	return SQL_NULLABLE;		/* everything should be nullable */
}


Int2
pgtype_auto_increment(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_INT2:
		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
		case PG_TYPE_FLOAT4:
		case PG_TYPE_MONEY:
		case PG_TYPE_BOOL:
		case PG_TYPE_FLOAT8:
		case PG_TYPE_INT8:
		case PG_TYPE_NUMERIC:

		case PG_TYPE_DATE:
		case PG_TYPE_TIME_WITH_TMZONE:
		case PG_TYPE_TIME:
		case PG_TYPE_ABSTIME:
		case PG_TYPE_DATETIME:
		case PG_TYPE_TIMESTAMP_NO_TMZONE:
		case PG_TYPE_TIMESTAMP:
			return FALSE;

		default:
			return -1;
	}
}


Int2
pgtype_case_sensitive(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_CHAR:

		case PG_TYPE_CHAR2:
		case PG_TYPE_CHAR4:
		case PG_TYPE_CHAR8:

		case PG_TYPE_VARCHAR:
		case PG_TYPE_BPCHAR:
		case PG_TYPE_TEXT:
		case PG_TYPE_NAME:
		case PG_TYPE_REFCURSOR:
			return TRUE;

		default:
			return FALSE;
	}
}


Int2
pgtype_money(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_MONEY:
			return TRUE;
		default:
			return FALSE;
	}
}


Int2
pgtype_searchable(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_CHAR:
		case PG_TYPE_CHAR2:
		case PG_TYPE_CHAR4:
		case PG_TYPE_CHAR8:

		case PG_TYPE_VARCHAR:
		case PG_TYPE_BPCHAR:
		case PG_TYPE_TEXT:
		case PG_TYPE_NAME:
		case PG_TYPE_REFCURSOR:
			return SQL_SEARCHABLE;

		default:
			if (conn && type == conn->lobj_type)
				return SQL_UNSEARCHABLE;
			return SQL_ALL_EXCEPT_LIKE;
	}
}


Int2
pgtype_unsigned(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_OID:
		case PG_TYPE_XID:
			return TRUE;

		case PG_TYPE_INT2:
		case PG_TYPE_INT4:
		case PG_TYPE_INT8:
		case PG_TYPE_NUMERIC:
		case PG_TYPE_FLOAT4:
		case PG_TYPE_FLOAT8:
		case PG_TYPE_MONEY:
			return FALSE;

		default:
			return -1;
	}
}


const char *
pgtype_literal_prefix(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_INT2:
		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
		case PG_TYPE_INT8:
		case PG_TYPE_NUMERIC:
		case PG_TYPE_FLOAT4:
		case PG_TYPE_FLOAT8:
		case PG_TYPE_MONEY:
			return NULL;

		default:
			return "'";
	}
}


const char *
pgtype_literal_suffix(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_INT2:
		case PG_TYPE_OID:
		case PG_TYPE_XID:
		case PG_TYPE_INT4:
		case PG_TYPE_INT8:
		case PG_TYPE_NUMERIC:
		case PG_TYPE_FLOAT4:
		case PG_TYPE_FLOAT8:
		case PG_TYPE_MONEY:
			return NULL;

		default:
			return "'";
	}
}


const char *
pgtype_create_params(const ConnectionClass *conn, OID type)
{
	switch (type)
	{
		case PG_TYPE_BPCHAR:
		case PG_TYPE_VARCHAR:
			return "max. length";
		case PG_TYPE_NUMERIC:
			return "precision, scale";
		default:
			return NULL;
	}
}


SQLSMALLINT
sqltype_to_default_ctype(const ConnectionClass *conn, SQLSMALLINT sqltype)
{
	/*
	 * from the table on page 623 of ODBC 2.0 Programmer's Reference
	 * (Appendix D)
	 */
	switch (sqltype)
	{
		case SQL_CHAR:
		case SQL_VARCHAR:
		case SQL_LONGVARCHAR:
		case SQL_DECIMAL:
		case SQL_NUMERIC:
#if (ODBCVER < 0x0300)
		case SQL_BIGINT:
			return SQL_C_CHAR;
#else
			return SQL_C_CHAR;
		case SQL_BIGINT:
			return ALLOWED_C_BIGINT;
#endif

#ifdef	UNICODE_SUPPORT
		case SQL_WCHAR:
		case SQL_WVARCHAR:
		case SQL_WLONGVARCHAR:
			if (!ALLOW_WCHAR(conn))
				return SQL_C_CHAR;
			return SQL_C_WCHAR;
#endif /* UNICODE_SUPPORT */

		case SQL_BIT:
			return SQL_C_BIT;

		case SQL_TINYINT:
			return SQL_C_STINYINT;

		case SQL_SMALLINT:
			return SQL_C_SSHORT;

		case SQL_INTEGER:
			return SQL_C_SLONG;

		case SQL_REAL:
			return SQL_C_FLOAT;

		case SQL_FLOAT:
		case SQL_DOUBLE:
			return SQL_C_DOUBLE;

		case SQL_BINARY:
		case SQL_VARBINARY:
		case SQL_LONGVARBINARY:
			return SQL_C_BINARY;

		case SQL_DATE:
			return SQL_C_DATE;

		case SQL_TIME:
			return SQL_C_TIME;

		case SQL_TIMESTAMP:
			return SQL_C_TIMESTAMP;

#if (ODBCVER >= 0x0300)
		case SQL_TYPE_DATE:
			return SQL_C_TYPE_DATE;

		case SQL_TYPE_TIME:
			return SQL_C_TYPE_TIME;

		case SQL_TYPE_TIMESTAMP:
			return SQL_C_TYPE_TIMESTAMP;
#endif /* ODBCVER */

#if (ODBCVER >= 0x0350)
		case SQL_GUID:
			if (conn->ms_jet)
				return SQL_C_CHAR;
			else
				return SQL_C_GUID;
#endif /* ODBCVER */

		default:
			/* should never happen */
			return SQL_C_CHAR;
	}
}

Int4
ctype_length(SQLSMALLINT ctype)
{
	switch (ctype)
	{
		case SQL_C_SSHORT:
		case SQL_C_SHORT:
			return sizeof(SWORD);

		case SQL_C_USHORT:
			return sizeof(UWORD);

		case SQL_C_SLONG:
		case SQL_C_LONG:
			return sizeof(SDWORD);

		case SQL_C_ULONG:
			return sizeof(UDWORD);

		case SQL_C_FLOAT:
			return sizeof(SFLOAT);

		case SQL_C_DOUBLE:
			return sizeof(SDOUBLE);

		case SQL_C_BIT:
			return sizeof(UCHAR);

		case SQL_C_STINYINT:
		case SQL_C_TINYINT:
			return sizeof(SCHAR);

		case SQL_C_UTINYINT:
			return sizeof(UCHAR);

		case SQL_C_DATE:
#if (ODBCVER >= 0x0300)
		case SQL_C_TYPE_DATE:
#endif /* ODBCVER */
			return sizeof(DATE_STRUCT);

		case SQL_C_TIME:
#if (ODBCVER >= 0x0300)
		case SQL_C_TYPE_TIME:
#endif /* ODBCVER */
			return sizeof(TIME_STRUCT);

		case SQL_C_TIMESTAMP:
#if (ODBCVER >= 0x0300)
		case SQL_C_TYPE_TIMESTAMP:
#endif /* ODBCVER */
			return sizeof(TIMESTAMP_STRUCT);

#if (ODBCVER >= 0x0350)
		case SQL_C_GUID:
			return sizeof(SQLGUID);
#endif /* ODBCVER */
#if (ODBCVER >= 0x0300)
		case SQL_C_INTERVAL_YEAR:
		case SQL_C_INTERVAL_MONTH:
		case SQL_C_INTERVAL_YEAR_TO_MONTH:
		case SQL_C_INTERVAL_DAY:
		case SQL_C_INTERVAL_HOUR:
		case SQL_C_INTERVAL_DAY_TO_HOUR:
		case SQL_C_INTERVAL_MINUTE:
		case SQL_C_INTERVAL_DAY_TO_MINUTE:
		case SQL_C_INTERVAL_HOUR_TO_MINUTE:
		case SQL_C_INTERVAL_SECOND:
		case SQL_C_INTERVAL_DAY_TO_SECOND:
		case SQL_C_INTERVAL_HOUR_TO_SECOND:
		case SQL_C_INTERVAL_MINUTE_TO_SECOND:
			return sizeof(SQL_INTERVAL_STRUCT);
#endif /* ODBCVER */

		case SQL_C_BINARY:
		case SQL_C_CHAR:
#ifdef	UNICODE_SUPPORT
		case SQL_C_WCHAR:
#endif /* UNICODE_SUPPORT */
			return 0;

		default:				/* should never happen */
			return 0;
	}
}