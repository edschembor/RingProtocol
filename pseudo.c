/**
 * TODO: Remove isFinished from token.h
 *       Come up with ending logic
 *       Last lowered?
 *       Holding array
 */

for(;;) {

	if(has_token) {
	
		/*If receive token, send ack*/
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
		if((tkn.aru > local_aru) || (/*You were last one to lower*/)) {
			tkn.aru = local_aru;
		}

		/*If you have any packets in the retrans in your packet holding
		 * array, send them*/
		/*Could definitely be more efficient*/
		for(i = 0; i < RETRANS_SIZE; i++) {
			for(j = 0; j < HOLDING_SIZE; j++) {
				if(holding[j]->packet_index == tkn.retransmission_request[i]) {
					sendto(ss, holding[j], sizeof(packet), 0, (struct sockaddr *)&send_addr, sizeof(send_addr) );
					break;
				}
			}
		}
		

		/*Add missing packets to the rtr array in the token*/
		for(i = 0; i < FRAME_SIZE; i++) {
			if((frame[i]->packet_index < last_written) && (frame[i]->packet_index+FRAME_SIZE <= num_packets)) {
				for(j = 0; j< RETRANS_SIZE; j++) {
					if(tkn.retransmission_request[i] == -1) {
						retransmission_request[i] = frame[i]->packet_index+FRAME_SIZE;
						break;
					}
					if(tkn.retransmission_request[i] == frame[i]->packet_index+FRAME_SIZE) {
						break;
					}
				}
			}
		}

		if(sent_packets < num_packets) {
			
			/*Update your frame so that it is filled and has no packets with
		 	* an index less than min(token.aru, previous token.aru) */
			/*sent_packets++;*/
		}else{
			
			if(local_ARU == tkn.sequence == tkn.aru) {
				/*FINISHING LOGIC*/
			}
			
		}

		/*Multicast all packets in your frame and update token*/
		for(i = 0; i < FRAME_SIZE; i++) {
		
			/*Multicast packet*/
			sendto( ss, frame[i], sizeof(packet), 0, (struct sockaddr *)&send_addr, sizeof(send_addr) );
			
			/*Update token */
			if((tkn.aru == tkn.sequence) && (local_aru==tkn.aru)){
				tkn.aru++;
			}
			if(packet->index > tkn.sequence) {
				token_sequence = packet->index;
			}
		}

		/*Set last_seen_ARU to the token's aru*/
		last_seen_aru = tkn.aru;

		/*Send the token to your neighbor*/
		send_token();

	}else{
		
		temp_mask = mask;
		num = select(FD_SETSIZE,&temp_mask,&dummy_mask,&dummy_mask,&timeout);
		if(num > 0) {
			bytes = recv_dbg(sr, (char *)buffer, PACKET_SIZE, 0);
			packet_type = buffer->type;

			/*If Receive type=0, you got data packet */
			if(packet_type == 0) {

				/*Cast to packet --->> Is it already packet?*/

				/*If the packet's index > local ARU, add to holder array*/

				/*Update local ARU*/

				/*Write if we have written_up_to+1 and update written_up_to*/
			
			}else if(packet_type==1) { /*If receive type=1, you got token*/
			
				/*Cast to token*/

				/*Set has_token = 1*/
			}

		}
	}
}
