/*
 * sql_postgresql.c		Postgresql rlm_sql driver
 *
 * Version:	$Id: sql_postgresql.c,v 1.38.4.1 2005/12/14 18:32:03 nbk Exp $
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Copyright 2000  The FreeRADIUS server project
 * Copyright 2000  Mike Machado <mike@innercite.com>
 * Copyright 2000  Alan DeKok <aland@ox.org>
 */

/*
 * April 2001:
 *
 * Use blocking queries and delete unused functions. In
 * rlm_sql_postgresql replace all functions that are not really used
 * with the not_implemented function.
 *
 * Add a new field to the rlm_sql_postgres_sock struct to store the
 * number of rows affected by a query because the sql module calls
 * finish_query before it retrieves the number of affected rows from the
 * driver
 *
 * Bernhard Herzog <bh@intevation.de>
 */

/* Modification of rlm_sql_mysql to handle postgres */

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include "radiusd.h"

#include        <libpq-fe.h>
#include	"rlm_sql.h"

typedef struct rlm_sql_postgres_sock {
   PGconn          *conn;
   PGresult        *result;
   int             cur_row;
   int             num_fields;
    int		   affected_rows;
   char            **row;
} rlm_sql_postgres_sock;

/* Prototypes */
static int sql_store_result(SQLSOCK * sqlsocket, SQL_CONFIG *config);
static int sql_num_fields(SQLSOCK * sqlsocket, SQL_CONFIG *config);

/* Internal function. Return true if the postgresql status value
 * indicates successful completion of the query. Return false otherwise
 */
static int
status_is_ok(ExecStatusType status)
{
	return status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK;
}


/* Internal function. Return the number of affected rows of the result
 * as an int instead of the string that postgresql provides */
static int
affected_rows(PGresult * result)
{
	return atoi(PQcmdTuples(result));
}

/* Internal function. Free the row of the current result that's stored
 * in the pg_sock struct. */
static void
free_result_row(rlm_sql_postgres_sock * pg_sock)
{
	int i;
	if (pg_sock->row != NULL) {
		for (i = pg_sock->num_fields-1; i >= 0; i--) {
			if (pg_sock->row[i] != NULL) {
				xfree(pg_sock->row[i]);
			}
		}
		xfree((char*)pg_sock->row);
		pg_sock->row = NULL;
		pg_sock->num_fields = 0;
	}
}

/*************************************************************************
 *
 *      Function: sql_check_error
 *
 *      Purpose: check the error to see if the server is down
 *
 *	Note: It is possible that something other than a connection error
 *	could cause PGRES_FATAL_ERROR. If that happens a reconnect will
 *	occur anyway. Not optimal, but I couldn't find a way to check it.
 *		 		Peter Nixon <codemonkey@peternixon.net>
 *

************************************************************************/
static int sql_check_error(int error) {
        switch(error) {
        case PGRES_FATAL_ERROR:
        case -1:
                radlog(L_DBG, "rlm_sql_postgresql: Postgresql check_error: %s, returning SQL_DOWN", PQresStatus(error));
                return SQL_DOWN;
                break;

        case PGRES_COMMAND_OK:
        case PGRES_TUPLES_OK:
        case 0:
                return 0;
                break;

        case PGRES_NONFATAL_ERROR:
        case PGRES_BAD_RESPONSE:
        default:
                radlog(L_DBG, "rlm_sql_postgresql: Postgresql check_error: %s received", PQresStatus(error));
                return -1;
                break;
        }
}


/*************************************************************************
 *
 *	Function: sql_create_socket
 *
 *	Purpose: Establish connection to the db
 *
 *************************************************************************/
static int sql_init_socket(SQLSOCK *sqlsocket, SQL_CONFIG *config) {
	char connstring[2048];
	char *port, *host;
	rlm_sql_postgres_sock *pg_sock;

	if (config->sql_server[0] != '\0') {
		host = " host=";
	} else {
		host = "";
	}

	if (config->sql_port[0] != '\0') {
		port = " port=";
	} else {
		port = "";
	}

	if (!sqlsocket->conn) {
		sqlsocket->conn = (rlm_sql_postgres_sock *)rad_malloc(sizeof(rlm_sql_postgres_sock));
		if (!sqlsocket->conn) {
			return -1;
		}
	}

	pg_sock = sqlsocket->conn;
	memset(pg_sock, 0, sizeof(*pg_sock));

	snprintf(connstring, sizeof(connstring),
			"dbname=%s%s%s%s%s user=%s password=%s",
			config->sql_db, host, config->sql_server,
			port, config->sql_port,
			config->sql_login, config->sql_password);
	pg_sock->row=NULL;
	pg_sock->result=NULL;
	pg_sock->conn=PQconnectdb(connstring);

	if (PQstatus(pg_sock->conn) == CONNECTION_BAD) {
		radlog(L_ERR, "rlm_sql_postgresql: Couldn't connect socket to PostgreSQL server %s@%s:%s", config->sql_login, config->sql_server, config->sql_db);
		radlog(L_ERR, "rlm_sql_postgresql: Postgresql error '%s'", PQerrorMessage(pg_sock->conn));
		PQfinish(pg_sock->conn);
		return SQL_DOWN;
	}

	return 0;
}

/*************************************************************************
 *
 *	Function: sql_query
 *
 *	Purpose: Issue a query to the database
 *
 *************************************************************************/
static int sql_query(SQLSOCK * sqlsocket, SQL_CONFIG *config, char *querystr) {

	rlm_sql_postgres_sock *pg_sock = sqlsocket->conn;

	if (config->sqltrace)
		radlog(L_DBG,"rlm_sql_postgresql: query:\n%s", querystr);

	if (pg_sock->conn == NULL) {
		radlog(L_ERR, "rlm_sql_postgresql: Socket not connected");
		return SQL_DOWN;
	}

	pg_sock->result = PQexec(pg_sock->conn, querystr);
		/* Returns a result pointer or possibly a NULL pointer.
		 * A non-NULL pointer will generally be returned except in
		 * out-of-memory conditions or serious errors such as inability
		 * to send the command to the backend. If a NULL is returned,
		 *  it should be treated like a PGRES_FATAL_ERROR result.
		 * Use PQerrorMessage to get more information about the error.
		 */
	if (!pg_sock->result)
	{
		radlog(L_ERR, "rlm_sql_postgresql: PostgreSQL Query failed Error: %s",
				PQerrorMessage(pg_sock->conn));
		return  SQL_DOWN;
	} else {
		ExecStatusType status = PQresultStatus(pg_sock->result);

		radlog(L_DBG, "rlm_sql_postgresql: Status: %s", PQresStatus(status));

		radlog(L_DBG, "rlm_sql_postgresql: affected rows = %s",
				PQcmdTuples(pg_sock->result));

		if (!status_is_ok(status))
			return sql_check_error(status);

		if (strncasecmp("select", querystr, 6) != 0) {
			/* store the number of affected rows because the sql module
			 * calls finish_query before it retrieves the number of affected
			 * rows from the driver */
			pg_sock->affected_rows = affected_rows(pg_sock->result);
			return 0;
		} else {
			if ((sql_store_result(sqlsocket, config) == 0)
					&& (sql_num_fields(sqlsocket, config) >= 0))
				return 0;
			else
				return -1;
		}
	}
}


/*************************************************************************
 *
 *	Function: sql_select_query
 *
 *	Purpose: Issue a select query to the database
 *
 *************************************************************************/
static int sql_select_query(SQLSOCK * sqlsocket, SQL_CONFIG *config, char *querystr) {
	return sql_query(sqlsocket, config, querystr);
}


/*************************************************************************
 *
 *	Function: sql_store_result
 *
 *	Purpose: database specific store_result function. Returns a result
 *               set for the query.
 *
 *************************************************************************/
static int sql_store_result(SQLSOCK * sqlsocket, SQL_CONFIG *config) {
	rlm_sql_postgres_sock *pg_sock = sqlsocket->conn;

	pg_sock->cur_row = 0;
	pg_sock->affected_rows = PQntuples(pg_sock->result);
	return 0;
}


/*************************************************************************
 *
 *      Function: sql_destroy_socket
 *
 *      Purpose: Free socket and private connection data
 *
 *************************************************************************/
static int sql_destroy_socket(SQLSOCK *sqlsocket, SQL_CONFIG *config)
{
        free(sqlsocket->conn);
	sqlsocket->conn = NULL;
        return 0;
}

/*************************************************************************
 *
 *	Function: sql_num_fields
 *
 *	Purpose: database specific num_fields function. Returns number
 *               of columns from query
 *
 *************************************************************************/
static int sql_num_fields(SQLSOCK * sqlsocket, SQL_CONFIG *config) {

	int num = 0;
	rlm_sql_postgres_sock *pg_sock = sqlsocket->conn;

	if (!(num = PQnfields(pg_sock->result))) {
		radlog(L_ERR, "rlm_sql_postgresql: PostgreSQL Error: Cannot get result");
		radlog(L_ERR, "rlm_sql_postgresql: PostgreSQL error: %s", PQerrorMessage(pg_sock->conn));
	}
	return num;
}


/*************************************************************************
 *
 *	Function: sql_fetch_row
 *
 *	Purpose: database specific fetch_row. Returns a SQL_ROW struct
 *               with all the data for the query in 'sqlsocket->row'. Returns
 *		 0 on success, -1 on failure, SQL_DOWN if 'database is down'.
 *
 *************************************************************************/
static int sql_fetch_row(SQLSOCK * sqlsocket, SQL_CONFIG *config) {

	int records, i, len;
	rlm_sql_postgres_sock *pg_sock = sqlsocket->conn;

	sqlsocket->row = NULL;

	if (pg_sock->cur_row >= PQntuples(pg_sock->result))
		return 0;

	free_result_row(pg_sock);

	records = PQnfields(pg_sock->result);
	pg_sock->num_fields = records;

	if ((PQntuples(pg_sock->result) > 0) && (records > 0)) {
		pg_sock->row = (char **)rad_malloc((records+1)*sizeof(char *));
		memset(pg_sock->row, '\0', (records+1)*sizeof(char *));

		for (i = 0; i < records; i++) {
			len = PQgetlength(pg_sock->result, pg_sock->cur_row, i);
			pg_sock->row[i] = (char *)rad_malloc(len+1);
			memset(pg_sock->row[i], '\0', len+1);
			strncpy(pg_sock->row[i], PQgetvalue(pg_sock->result, pg_sock->cur_row,i),len);
		}
		pg_sock->cur_row++;
		sqlsocket->row = pg_sock->row;
		return 0;
	} else {
		return 0;
	}
}



/*************************************************************************
 *
 *	Function: sql_free_result
 *
 *	Purpose: database specific free_result. Frees memory allocated
 *               for a result set
 *
 *************************************************************************/
static int sql_free_result(SQLSOCK * sqlsocket, SQL_CONFIG *config) {

	rlm_sql_postgres_sock *pg_sock = sqlsocket->conn;

	if (pg_sock->result) {
		PQclear(pg_sock->result);
		pg_sock->result = NULL;
	}
#if 0
	/*
	 *  Commented out because it appears to free memory too early.
	 */
	free_result_row(pg_sock);
#endif

	return 0;
}



/*************************************************************************
 *
 *	Function: sql_error
 *
 *	Purpose: database specific error. Returns error associated with
 *               connection
 *
 *************************************************************************/
static char *sql_error(SQLSOCK * sqlsocket, SQL_CONFIG *config) {

	rlm_sql_postgres_sock *pg_sock = sqlsocket->conn;

	return PQerrorMessage(pg_sock->conn);
}


/*************************************************************************
 *
 *	Function: sql_close
 *
 *	Purpose: database specific close. Closes an open database
 *               connection
 *
 *************************************************************************/
static int sql_close(SQLSOCK * sqlsocket, SQL_CONFIG *config) {

	rlm_sql_postgres_sock *pg_sock = sqlsocket->conn;

	if (!pg_sock->conn) return 0;

	PQfinish(pg_sock->conn);
	pg_sock->conn = NULL;

	return 0;
}


/*************************************************************************
 *
 *	Function: sql_finish_query
 *
 *	Purpose: End the query, such as freeing memory
 *
 *************************************************************************/
static int sql_finish_query(SQLSOCK * sqlsocket, SQL_CONFIG *config) {

	return sql_free_result(sqlsocket, config);
}



/*************************************************************************
 *
 *	Function: sql_finish_select_query
 *
 *	Purpose: End the select query, such as freeing memory or result
 *
 *************************************************************************/
static int sql_finish_select_query(SQLSOCK * sqlsocket, SQL_CONFIG *config) {

	return sql_free_result(sqlsocket, config);
}


/*************************************************************************
 *
 *	Function: sql_affected_rows
 *
 *	Purpose: Return the number of rows affected by the last query.
 *
 *************************************************************************/
static int sql_affected_rows(SQLSOCK * sqlsocket, SQL_CONFIG *config) {
	rlm_sql_postgres_sock *pg_sock = sqlsocket->conn;

	return pg_sock->affected_rows;
}


static int NEVER_RETURNS
not_implemented(SQLSOCK * sqlsocket, SQL_CONFIG *config)
{
	radlog(L_ERR, "sql_postgresql: calling unimplemented function");
	exit(1);
}


/* Exported to rlm_sql */
rlm_sql_module_t rlm_sql_postgresql = {
	"rlm_sql_postgresql",
	sql_init_socket,
	sql_destroy_socket,
	sql_query,
	sql_select_query,
	not_implemented, /* sql_store_result */
	not_implemented, /* sql_num_fields */
	not_implemented, /* sql_num_rows */
	sql_fetch_row,
	not_implemented, /* sql_free_result */
	sql_error,
	sql_close,
	sql_finish_query,
	sql_finish_select_query,
	sql_affected_rows,
};
