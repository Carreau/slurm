/*****************************************************************************\
 *  job_info.c - get/print the job state information of slurm
 *****************************************************************************
 *  Copyright (C) 2002 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Moe Jette <jette1@llnl.gov> et. al.
 *  UCRL-CODE-2002-040.
 *  
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.llnl.gov/linux/slurm/>.
 *  
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with ConMan; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <src/api/slurm.h>
#include <src/common/slurm_protocol_api.h>

/* slurm_print_job_info_msg - output information about all Slurm jobs */
void 
slurm_print_job_info_msg ( FILE* out, job_info_msg_t * job_info_msg_ptr )
{
	int i;
	job_table_t * job_ptr = job_info_msg_ptr -> job_array ;

	fprintf( out, "Jobs updated at %ld, record count %d\n",
		(long) job_info_msg_ptr ->last_update, job_info_msg_ptr->record_count);

	for (i = 0; i < job_info_msg_ptr-> record_count; i++) 
	{
		slurm_print_job_table ( out, & job_ptr[i] ) ;
	}
}

/* slurm_print_job_table - output information about a specific Slurm job */
void
slurm_print_job_table ( FILE* out, job_table_t * job_ptr )
{
	int j;

	fprintf ( out, "JobId=%u UserId=%u ", job_ptr->job_id, job_ptr->user_id);
	fprintf ( out, "JobState=%s TimeLimit=%u ", job_state_string(job_ptr->job_state), job_ptr->time_limit);
	fprintf ( out, "Priority=%u Partition=%s\n", job_ptr->priority, job_ptr->partition);
	fprintf ( out, "   Name=%s NodeList=%s ", job_ptr->name, job_ptr->nodes);
	fprintf ( out, "StartTime=%x EndTime=%x ", (uint32_t) job_ptr->start_time, (uint32_t) job_ptr->end_time);

	fprintf ( out, "NodeListIndecies=");
	for (j = 0; job_ptr->node_inx; j++) {
		if (j > 0)
			fprintf( out, ",%d", job_ptr->node_inx[j]);
		else
			fprintf( out, "%d", job_ptr->node_inx[j]);
		if (job_ptr->node_inx[j] == -1)
			break;
	}
	fprintf( out, "\n");

	fprintf ( out, "   ReqProcs=%u ReqNodes=%u ", job_ptr->num_procs, job_ptr->num_nodes);
	fprintf ( out, "Shared=%u Contiguous=%u ", job_ptr->shared, job_ptr->contiguous);
	fprintf ( out, "MinProcs=%u MinMemory=%u ", job_ptr->min_procs, job_ptr->min_memory);
	fprintf ( out, "MinTmpDisk=%u\n", job_ptr->min_tmp_disk);
	fprintf ( out, "   ReqNodeList=%s Features=%s ", job_ptr->req_nodes, job_ptr->features);
	fprintf ( out, "JobScript=%s ", job_ptr->job_script);
	fprintf ( out, "ReqNodeListIndecies=");
	for (j = 0; job_ptr->req_node_inx; j++) {
		if (j > 0)
			fprintf( out, ",%d", job_ptr->req_node_inx[j]);
		else
			fprintf( out, "%d", job_ptr->req_node_inx[j]);
		if (job_ptr->req_node_inx[j] == -1)
			break;
	}
	fprintf( out, "\n\n");
}

/* slurm_load_jobs - issue RPC to get Slurm job state information if changed since update_time */
int
slurm_load_jobs (time_t update_time, job_info_msg_t **job_info_msg_pptr)
{
        int msg_size ;
        int rc ;
        slurm_fd sockfd ;
        slurm_msg_t request_msg ;
        slurm_msg_t response_msg ;
        last_update_msg_t last_time_msg ;
	return_code_msg_t * slurm_rc_msg ;

        /* init message connection for message communication with controller */
	if ( ( sockfd = slurm_open_controller_conn ( ) ) == SLURM_SOCKET_ERROR ) {
		slurm_seterrno ( SLURM_COMMUNICATIONS_CONNECTION_ERROR );
		return SLURM_SOCKET_ERROR ;
	}

        /* send request message */
        last_time_msg . last_update = update_time ;
        request_msg . msg_type = REQUEST_JOB_INFO ;
        request_msg . data = &last_time_msg ;
	if ( ( rc = slurm_send_controller_msg ( sockfd , & request_msg ) ) == SLURM_SOCKET_ERROR ) {
		slurm_seterrno ( SLURM_COMMUNICATIONS_SEND_ERROR );
		return SLURM_SOCKET_ERROR ;
	}

        /* receive message */
	if ( ( msg_size = slurm_receive_msg ( sockfd , & response_msg ) ) == SLURM_SOCKET_ERROR ) {
		slurm_seterrno ( SLURM_COMMUNICATIONS_RECEIVE_ERROR );
		return SLURM_SOCKET_ERROR ;
	}

        /* shutdown message connection */
	if ( ( rc = slurm_shutdown_msg_conn ( sockfd ) ) == SLURM_SOCKET_ERROR ) {
		slurm_seterrno ( SLURM_COMMUNICATIONS_SHUTDOWN_ERROR );
		return SLURM_SOCKET_ERROR ;
	}
	if ( msg_size )
		return msg_size;

	switch ( response_msg . msg_type )
	{
		case RESPONSE_JOB_INFO:
        		 *job_info_msg_pptr = ( job_info_msg_t * ) response_msg . data ;
		        return SLURM_PROTOCOL_SUCCESS ;
			break ;
		case RESPONSE_SLURM_RC:
			slurm_rc_msg = ( return_code_msg_t * ) response_msg . data ;
			rc = slurm_rc_msg->return_code;
			slurm_free_return_code_msg ( slurm_rc_msg );	
			if (rc) {
				slurm_seterrno ( rc );
				return SLURM_PROTOCOL_ERROR;
			}
			break ;
		default:
			slurm_seterrno ( SLURM_UNEXPECTED_MSG_ERROR );
			return SLURM_PROTOCOL_ERROR;
			break ;
	}

        return SLURM_PROTOCOL_SUCCESS ;
}

