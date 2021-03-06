/********************************************************************
 * SQLSetPos
 *
 **********************************************************************
 *
 * This code was created by Peter Harvey (mostly during Christmas 98/99).
 * This code is LGPL. Please ensure that this message remains in future
 * distributions and uses of this code (thats about all I get out of it).
 * - Peter Harvey pharvey@codebydesign.com
 *
 ********************************************************************/

#include "driver.h"

SQLRETURN SQLSetPos(	SQLHSTMT  		hDrvStmt,
							SQLUSMALLINT	nRow,
							SQLUSMALLINT	nOperation,
							SQLUSMALLINT	nLockType )
{
    HDRVSTMT hStmt	= (HDRVSTMT)hDrvStmt;

	/* SANITY CHECKS */
    if( hStmt == SQL_NULL_HSTMT )
        return SQL_INVALID_HANDLE;

	sprintf( hStmt->szSqlMsg, "hStmt = $%08lX", hStmt );
    logPushMsg( hStmt->hLog, __FILE__, __FILE__, __LINE__, LOG_WARNING, LOG_WARNING, hStmt->szSqlMsg );

	/* OK */

	switch ( nOperation )
	{
	case SQL_POSITION:
		break;
	case SQL_REFRESH:
		break;
	case SQL_UPDATE:
		break;
	case SQL_DELETE:
		break;
	default:
		sprintf( hStmt->szSqlMsg, "SQL_ERROR Invalid nOperation=%d", nOperation );
		logPushMsg( hStmt->hLog, __FILE__, __FILE__, __LINE__, LOG_WARNING, LOG_WARNING, hStmt->szSqlMsg );
		return SQL_ERROR;
	}

	switch ( nLockType )
	{
	case SQL_LOCK_NO_CHANGE:
		break;
	case SQL_LOCK_EXCLUSIVE:
		break;
	case SQL_LOCK_UNLOCK:
		break;
	default:
		sprintf( hStmt->szSqlMsg, "SQL_ERROR Invalid nLockType=%d", nLockType );
		logPushMsg( hStmt->hLog, __FILE__, __FILE__, __LINE__, LOG_WARNING, LOG_WARNING, hStmt->szSqlMsg );
		return SQL_ERROR;
	}

    /************************
     * REPLACE THIS COMMENT WITH SOMETHING USEFULL
     ************************/
    logPushMsg( hStmt->hLog, __FILE__, __FILE__, __LINE__, LOG_WARNING, LOG_WARNING, "SQL_ERROR This function not supported" );


	return SQL_ERROR;
}


