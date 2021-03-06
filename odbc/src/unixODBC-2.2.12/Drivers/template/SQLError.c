/**********************************************************************
 * SQLError (deprecated see SQLGetDiagRec)
 *
 **********************************************************************
 *
 * This code was created by Peter Harvey (mostly during Christmas 98/99).
 * This code is LGPL. Please ensure that this message remains in future
 * distributions and uses of this code (thats about all I get out of it).
 * - Peter Harvey pharvey@codebydesign.com
 *
 **********************************************************************/

#include "driver.h"

SQLRETURN SQLError( SQLHENV     hDrvEnv,
					SQLHDBC     hDrvDbc,
					SQLHSTMT    hDrvStmt,
					SQLCHAR   	*szSqlState,
					SQLINTEGER  *pfNativeError,
					SQLCHAR   	*szErrorMsg,
					SQLSMALLINT nErrorMsgMax,
					SQLSMALLINT	*pcbErrorMsg )
{
	HDRVENV		hEnv	= (HDRVENV)hDrvEnv;
	HDRVDBC		hDbc	= (HDRVDBC)hDrvDbc;
    HDRVSTMT	hStmt	= (HDRVSTMT)hDrvStmt;
	char    *pszState       = NULL;			 /* pointer to status code   */
	char    *pszErrMsg      = NULL;			 /* pointer to error message */
	char	szMsgHdr[1024];
	int		nCode;

	/* SANITY CHECKS */
    if( hEnv == SQL_NULL_HENV && hDbc == SQL_NULL_HDBC && hStmt == SQL_NULL_HSTMT )
        return SQL_INVALID_HANDLE;

	/* DEFAULTS */
	szSqlState[0]	= '\0';
	*pfNativeError	= 0;
	szErrorMsg[0]	= '\0';
	*pcbErrorMsg	= 0;

	/* STATEMENT */
    if( hStmt != SQL_NULL_HENV )
	{
		if ( logPopMsg( hStmt->hLog, szMsgHdr, &nCode,(char*) hStmt->szSqlMsg ) != LOG_SUCCESS )
			return SQL_NO_DATA;
		sprintf((char*) szErrorMsg, "%s%s", szMsgHdr, hStmt->szSqlMsg );
		*pcbErrorMsg = strlen((char*) szErrorMsg );
		return SQL_SUCCESS;
	}

	/* CONNECTION */
    if( hDbc != SQL_NULL_HDBC )
	{
		if ( logPopMsg( hDbc->hLog, szMsgHdr, &nCode,(char*) hDbc->szSqlMsg ) != LOG_SUCCESS )
			return SQL_NO_DATA;
		sprintf((char*) szErrorMsg, "%s%s", szMsgHdr, hDbc->szSqlMsg );
		*pcbErrorMsg = strlen((char*) szErrorMsg );
		return SQL_SUCCESS;
	}

	/* ENVIRONMENT */
    if( hEnv != SQL_NULL_HSTMT )
	{
		if ( logPopMsg( hEnv->hLog, szMsgHdr, &nCode,(char*) hEnv->szSqlMsg ) != LOG_SUCCESS )
			return SQL_NO_DATA;
		sprintf((char*) szErrorMsg, "%s%s", szMsgHdr, hEnv->szSqlMsg );
		*pcbErrorMsg = strlen((char*) szErrorMsg );
		return SQL_SUCCESS;
	}

	return SQL_NO_DATA;
}


