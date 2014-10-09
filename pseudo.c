for(;;) {

	if(has_token) {
	
		/*Logic for updating the token*/

		/*If you have any packets in the retrans in your packet holding
		 * array, send them*/

		/*Add missing packets to the rtr array in the token*/

		/*If sent_packets < num_packets*/

			/*Update your frame so that it is filled and has no packets with
		 	* an index less than min(token.aru, previous token.aru) */

		/*else*/

			/*if local ARU = token.sq = token.aru*/

				/*FINISHING LOGIC*/

		/*Multicast all packets in your frame*/

		/*Set last_seen_ARU to the token's aru*/

		/*Send the token to your neighbor*/
		/*When you get ack, has_token = 0*/

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
