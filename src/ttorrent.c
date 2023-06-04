
// Trivial Torrent

// TODO: some includes here

#include "file_io.h"
#include "logger.h"
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>

// TODO: hey!? what is this?

/**
 * This is the magic number (already stored in network byte order).
 * See https://en.wikipedia.org/wiki/Magic_number_(programming)#In_protocols
 */
static const uint32_t MAGIC_NUMBER = 0xde1c3233; // = htonl(0x33321cde);

static const uint8_t MSG_REQUEST = 0;
static const uint8_t MSG_RESPONSE_OK = 1;
static const uint8_t MSG_RESPONSE_NA = 2;

enum { RAW_MESSAGE_SIZE = 13 };

/**
 * Main function.
 */
int main(int argc, char **argv) {

	set_log_level(LOG_DEBUG);

	log_printf(LOG_INFO, "Trivial Torrent (build %s %s) by %s", __DATE__, __TIME__, "Jill Areny Palma Garro and Adrian Gomez Roldan");

	// ==========================================================================
	// Parse command line
	// ==========================================================================

	// TODO: some magical lines of code here that call other functions and do various stuff.
					
	if (argc == 2) {		
		char* nom_file = argv[1];	
		log_printf(LOG_INFO,"clieent");
		if (access((const char *)nom_file, F_OK) >= 0) {
			log_printf(LOG_INFO,"stat ok");
			char* nom_download = malloc(sizeof(char) * strlen(nom_file));
			size_t pos = (size_t)(strstr(nom_file,".") - nom_file);
			strncpy(nom_download,nom_file,pos);
			struct torrent_t torrent;	
			if (create_torrent_from_metainfo_file(nom_file,&torrent,(const char *)nom_download) >= 0) {	
				log_printf(LOG_INFO,"create_torrent_from_metainfo_file ok");		
				uint64_t * bloques_mal = malloc(sizeof(uint64_t) * torrent.block_count);
				uint64_t bloques_mal_size = 0;
				for (uint64_t i = 0; i < torrent.block_count; i++) {
					if(!torrent.block_map[i]) {
						bloques_mal_size++;
						bloques_mal[bloques_mal_size-1] = i;
						log_printf(LOG_INFO,"bloque mal %u, total= %u",i,bloques_mal_size);
					}
				}		
				uint64_t bloquescorregidos = 0;
				for (uint64_t i = 0; i < torrent.peer_count; i++) {
					if (bloquescorregidos == bloques_mal_size) {
							log_printf(LOG_INFO,"todos bloques corregidos, total= %u",bloquescorregidos);
							break;
					}
				
					int sock = socket(AF_INET,SOCK_STREAM,0);				
					if (sock >= 0) {
						log_printf(LOG_INFO,"socket ok %u" , i);				
						struct sockaddr_in con;						
						con.sin_family = AF_INET;
						con.sin_port = torrent.peers[i].peer_port;	
						con.sin_addr.s_addr = htonl(((uint32_t)torrent.peers[i].peer_address[0] << 24) |
                    							  ((uint32_t)torrent.peers[i].peer_address[1] << 16) |
                    						      ((uint32_t)torrent.peers[i].peer_address[2] << 8) |
                     							  ((uint32_t)torrent.peers[i].peer_address[3]));
						int cas = connect(sock,(struct sockaddr*)&con,sizeof(con));
						if (cas >= 0) {
							log_printf(LOG_INFO,"connect ok, peer %i",i);
							for (uint64_t j = 0; j < bloques_mal_size;j++) {			
								//mal descargado
								uint64_t numero_bloque = bloques_mal[j];				
								if (!torrent.block_map[numero_bloque]) {
									log_printf(LOG_INFO,"buscando bloque mal %u",bloques_mal[j]);
									uint8_t buff_s[RAW_MESSAGE_SIZE];	
									uint32_t magic = htonl(MAGIC_NUMBER);
									memcpy(buff_s, &magic, sizeof(magic));
									buff_s[4] = MSG_REQUEST;		
									uint32_t block_index_high = htonl((uint32_t)(numero_bloque >> 32));
									uint32_t block_index_low = htonl((uint32_t)(numero_bloque & 0xffffffff));
									memcpy(&buff_s[5], &block_index_high, sizeof(block_index_high));
									memcpy(&buff_s[9], &block_index_low, sizeof(block_index_low));										
									ssize_t cc = send(sock,buff_s,RAW_MESSAGE_SIZE,0);
									if (cc >= 0) {
										log_printf(LOG_INFO,"send ok peer %u bloque %u",i,j);
										uint8_t buff_r[RAW_MESSAGE_SIZE];
										if(recv(sock,buff_r,sizeof(buff_r), MSG_WAITALL) >= 0){
											log_printf(LOG_INFO,"recv msg ok  peer %u bloque %u",i,j);			
											if(buff_r[4] == MSG_RESPONSE_OK){
												log_printf(LOG_INFO,"buff[4] msg ok peer %u bloque %u",i,j);
												uint32_t mgnumber = 0;
												memcpy(&mgnumber, &buff_r, sizeof(uint32_t));
												if (mgnumber == magic) {
													log_printf(LOG_INFO,"magic number ok peer %u bloque %u", i,j);
													uint64_t tamaño_blq = get_block_size(&torrent, numero_bloque);
													uint8_t bloque_respuesta[tamaño_blq];
													if (recv(sock,&bloque_respuesta,sizeof(bloque_respuesta),MSG_WAITALL) >= 0) {
														log_printf(LOG_INFO,"recv bloque ok peer %u bloque %u", i,j);
														struct block_t bloque;
														memcpy(bloque.data,bloque_respuesta,tamaño_blq);
														bloque.size = tamaño_blq;
														if (store_block(&torrent, numero_bloque, &bloque) >= 0) {
															bloquescorregidos++;
															log_printf(LOG_INFO,"store ok peer %u bloque %u, bloques corregidos %u", i,j, bloquescorregidos);
														}
													}				
												}
											}
										} 
									}																			
								}
							}
						}
						if (close(sock) != -1)											
							log_printf(LOG_INFO,"close ok peer %u",i);
					}	
				}
				if (destroy_torrent(&torrent) >= 0)
					log_printf(LOG_INFO,"torrent destruido ");
			}
		}
	}else{
		log_printf(LOG_INFO,"servera");
		char* nom_file = argv[3];	
		if (access((const char *)nom_file, F_OK) >= 0) {
			log_printf(LOG_INFO,"stat ok");
			char* nom_download = malloc(sizeof(char)*strlen(nom_file));
			size_t pos = (size_t)(strstr(nom_file,".") - nom_file);
			strncpy(nom_download,nom_file,pos);
			struct torrent_t torrent;	
			if (create_torrent_from_metainfo_file(nom_file,&torrent,(const char *)nom_download) >= 0) {
				log_printf(LOG_INFO,"create_torrent_from_metainfo_file ok");
				uint64_t * bloques_mal = malloc(sizeof(uint64_t)*torrent.block_count);
				uint64_t bloques_mal_size = 0;
				for (uint64_t i = 0; i < torrent.block_count;i++) {
					if (!torrent.block_map[i]) {
						bloques_mal_size++;
						bloques_mal[bloques_mal_size-1] = i;
						log_printf(LOG_INFO,"bloque mal %u, total= %u",i,bloques_mal_size);
					}
				}
				int sock = socket(AF_INET,SOCK_STREAM,0);
				if (sock >= 0) {
					log_printf(LOG_INFO,"socket ok");
					int flagsaa = fcntl(sock, F_GETFL, 0);
					if (flagsaa >= 0) {
					log_printf(LOG_INFO,"fncl flags ok");
		        if (fcntl(sock, F_SETFL, flagsaa | O_NONBLOCK) >= 0) {
						log_printf(LOG_INFO,"fncl sock ok");
						struct sockaddr_in server;						
						server.sin_family = AF_INET;
						server.sin_port = htons((uint16_t)atoi(argv[2]));	
						server.sin_addr.s_addr = htonl(INADDR_ANY);
						if (bind(sock,(struct sockaddr*)& server,sizeof(server)) >= 0) {
							log_printf(LOG_INFO,"bind ok");
							if (listen(sock,SOMAXCONN) >= 0) {
								log_printf(LOG_INFO,"listen ok");
								struct pollfd clients[100];
		            clients[0].fd = sock;
		            clients[0].events = POLLIN;
		            nfds_t num_clients = 1;
								while(1){
									int poll_result = poll(clients, num_clients, -1);
		              if (poll_result > 0) {
										if (clients[0].revents & POLLIN) {
											log_printf(LOG_INFO,"pollin ok");					   
											struct sockaddr_in client_addr;
											socklen_t addr_len = sizeof(client_addr);
											int new_sock = accept(sock, (struct sockaddr*)&client_addr, &addr_len);
											if (new_sock >= 0) {
													log_printf(LOG_INFO,"accept ok");  
													int flags = fcntl(new_sock, F_GETFL, 0);
												if (flags >= 0) {
												log_printf(LOG_INFO,"flags newsock ok"); 
													if (fcntl(new_sock, F_SETFL, flags | O_NONBLOCK) >= 0) {
													log_printf(LOG_INFO,"flags newsock ok 2"); 
														clients[num_clients].fd = new_sock;
														clients[num_clients].events = POLLIN;
														num_clients++;
													}
												}
											}
										}
			               log_printf(LOG_INFO,"num_clietnst %u",num_clients);
					           for (nfds_t i = 1; i < num_clients; i++) {
												uint8_t buff_r[RAW_MESSAGE_SIZE];
				                if (clients[i].revents & POLLIN) {
				                		ssize_t aux=recv(clients[i].fd,buff_r,RAW_MESSAGE_SIZE, MSG_WAITALL)	;					
														if ( aux> 0) {
															log_printf(LOG_INFO,"recv ok");
															clients[i].events = POLLOUT;
														}else { 
															if (aux==0){
            										close(clients[i].fd);
																clients[i] = clients[num_clients - 1];
																num_clients--;
																i--;
															}else{
																log_printf(LOG_INFO,"error recv");
															}	
														}
													}
													if (clients[i].revents & POLLOUT) {
														log_printf(LOG_INFO,"pollout ok");
														if (buff_r[4] == MSG_REQUEST) {
																log_printf(LOG_INFO,"request ok");
																uint32_t mgnumber = 0;
																memcpy(&mgnumber, &buff_r, sizeof(uint32_t));
																if (ntohl(mgnumber) == MAGIC_NUMBER) {
																	log_printf(LOG_INFO,"magic ok");
																	uint32_t block_index_high = 0;
																	uint32_t block_index_low = 0;
																	memcpy(&block_index_high, &buff_r[5], sizeof(block_index_high));
																	memcpy(&block_index_low, &buff_r[9], sizeof(block_index_low));			
																	block_index_high = ntohl(block_index_high);
																	uint64_t bloque_num = ((uint64_t)block_index_high << 32) | ntohl(block_index_low);
																	log_printf(LOG_INFO,"numero_bloque %u",bloque_num);
																	if (torrent.block_map[bloque_num]) {
																		log_printf(LOG_INFO,"block map ok,  msg_request");
																		struct block_t block_return;
																		if (load_block((struct torrent_t *)&torrent,bloque_num,(struct block_t *)&block_return) >=0 ) {
																			log_printf(LOG_INFO,"load blque ok");
																			buff_r[4] = MSG_RESPONSE_OK;
																			if (send(clients[i].fd,buff_r,sizeof(buff_r),0) < 0) {
																				log_printf(LOG_INFO,"send msg error");
																			}
																			uint8_t buffer_bloque[block_return.size];
																			uint64_t tamaño_blq = get_block_size(&torrent, bloque_num);
																			memcpy(&buffer_bloque, &block_return.data, tamaño_blq);
																			if (send(clients[i].fd,buffer_bloque,sizeof(buffer_bloque),0) < 0) {
																				log_printf(LOG_INFO,"send bloque error");
																			}else {
																			clients[i].events = POLLIN;
																			}
																		}
																	}
																	else {
																		log_printf(LOG_INFO,"load blque no ok, msg_NA");
																		buff_r[4] = MSG_RESPONSE_NA;
																		
																		if (send(clients[i].fd,buff_r,RAW_MESSAGE_SIZE,0) < 0) {
																			log_printf(LOG_INFO,"send msg_NA error");
																		}	
																	}
																}	
															}
													}
												}
				            	}					
										}
									}
								}
							}
						}	
					}		
					if (close(sock) >= 0) {
						log_printf(LOG_INFO,"close sock");
					}else {
						log_printf(LOG_INFO,"error close sock");
					}
				}
				if (destroy_torrent(&torrent) >= 0){
					log_printf(LOG_INFO,"torrent destruido ");
				}
				
			}
		}			
		
 
	// The following statements most certainly will need to be deleted at some point...
	(void) argc;
	(void) argv;
	(void) MAGIC_NUMBER;
	(void) MSG_REQUEST;
	(void) MSG_RESPONSE_NA;
	(void) MSG_RESPONSE_OK;	

	return 0;
}
