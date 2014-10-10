/**
 * TODO: Remove isFinished from token.h
 *       Come up with ending logic
 *       Last lowered?
 *       Holding array
 */

for(;;) {

	if(has_token) {
	
		/*If receive token, send ack*/
		/*Set timeout?*/
		num = select( FD_SETSIZE, &temp_mask, &dummy_mask, &dummy_mask, &timeout);
        if (num > 0) {
            bytes = recv_dbg( sr, (char *) buffer, PACKET_SIZE, 0 );
            packet_type = buffer->type;

            if (packet_type == 1) {
            	sendto( ss, tkn_ack, sizeof(token), 0,  (struct sockaddr *)&(((token *)buffer)->from_addr), 
            		sizeof(((token *)buffer)->from_addr) );
            }
        }
	
		/*Logic for updating the token*/
		if((token->aru > local_aru) || (/*You were last one to lower*/)) {
			token->aru = local_aru;
		}

		/*If you have any packets in the retrans in your packet holding
		 * array, send them*/
		for(i = 0; i < RETRANS_SIZE; i++) {
			if(holding[i%HOLDING_SIZE]->packet_index == token->retransmission_request[i]) {
				sendto( ss, holding[i%HOLDING_SIZE], sizeof(packet), 0, (struct sockaddr *)&send_addr, 
					sizeof(send_addr) );
			}
		}
		

		/*Add missing packets to the rtr array in the token*/
		for(i = 0; i < FRAME_SIZE; i++) {
			if(frame[i]->packet_index < last_written) {
				/*Add to rtr if not already in it*/
			}
		}

		if(sent_packets < num_packets) {
			
			/*Update your frame so that it is filled and has no packets with
		 	* an index less than min(token.aru, previous token.aru) */
		}else{
			
			if(local_ARU == token.sq == token.aru) {
				/*FINISHING LOGIC*/
			}
			
		}

		/*Multicast all packets in your frame and update token*/
		for(i = 0; i < FRAME_SIZE; i++) {
		
			/*Multicast packet*/
			sendto( ss, frame[i], sizeof(packet), 0, (struct sockaddr *)&send_addr, sizeof(send_addr) );
			
			/*Update token*/
			if((token->aru == token->sequence) && (local_aru == token->aru) ) {
				token->aru++;
			}
			if(packet->index > token->sequence) {
				token_sequence = packet->index;
			}	
		}

		/*Set last_seen_ARU to the token's aru*/

		/*Send the token to your neighbor*/
		
		/*Wait for ack*/
		
		/*has_token = 0*/

	}else{
		/*If Receive type=0, you got data packet */
			
			/*Place packet in the holding array*/

			/*If the packet's index > local ARU, add to holder array*/

			/*Update local ARU*/

			/*Write if we have written_up_to+1  and update written_up_to*/

		/*If receive type=1, you got token*/
			
			/*Send Ack*/

			/*Set has_token = 1*/

	}

}
