#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
// stdlib.h for exit()
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <time.h>
#include <unordered_map>
#include <cstring>
#include <sstream>
#include <chrono>

#include <fstream>


#include "../starter_code/DNSHeader.h"
#include "../starter_code/DNSQuestion.h"
#include "../starter_code/DNSRecord.h"




using namespace std;

string getServerIPfromDNS(int dns_port, char* dns_ip, char* domain) {
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1){
		perror("socket error");
		return "";
	}
	struct sockaddr_in addr_DNS_Server;
	addr_DNS_Server.sin_family = AF_INET;
	addr_DNS_Server.sin_port = dns_port;
	addr_DNS_Server.sin_addr.s_addr = inet_addr(dns_ip);
	if(connect(sock, (struct sockaddr *)&addr_DNS_Server, sizeof(addr_DNS_Server)) == -1){
		close(sock);
		perror("client: connect");
		return "";
	}

	DNSHeader request;
	request.ID = 1;
	request.QR = 0;
	request.OPCODE = 1;
	request.AA = 1;
	request.TC = 1;
	request.RD = 1;
	request.RA = 1;
	request.Z = '1';
	request.RCODE = 1;
	request.QDCOUNT = 1;
	request.ANCOUNT = 1;
	request.NSCOUNT = 1;
	request.ARCOUNT = 1;

	char buff[sizeof(request)];
	memcpy(buff, &request, sizeof(request));

	send(sock, buff, sizeof(buff), 0);

	char break_point = 0x4;
	send(sock, &break_point, 1, 0);

	DNSQuestion question;
	strncpy(question.QNAME, domain, sizeof(question.QNAME));
	char buff2[sizeof(question)];
	memcpy(buff2, &question, sizeof(question));
	send(sock, buff2, sizeof(buff2), 0);


	char break_point2 = 0x4;
	send(sock, &break_point2, 1, 0);

	char buf;
	string dns_str;
	while(true){
		int byteReceived = recv(sock, &buf, 1, 0);
		if(byteReceived < 0){
			perror("recv");
		}
		//if proxy end send message
		if(buf == 0x4){
			break;
		}
		//still rece
		dns_str += buf;
	}

	DNSHeader dnsHeader;
	memcpy(&dnsHeader, dns_str.c_str(), dns_str.size());
	if(dnsHeader.RCODE == '0'){
		string dns_str;
		while(true){
			int byteReceived = recv(sock, &buf, 1, 0);
			if(byteReceived < 0){
				perror("recv");
			}
			//if proxy end send message
			if(buf == 0x4){
				break;
			}
			//still rece
			dns_str += buf;
		}
		DNSRecord dns_anwser;
		memcpy(&dns_anwser, dns_str.c_str(), dns_str.size());
		// cout<<"the IP is: "<<dns_anwser.NAME<<endl;
		close(sock);
		return string(dns_anwser.NAME);
	}else{
		close(sock);
		return "";
	}
	


} 

string receiveHeader(int server){
	string header = "";
	char buf;
	while(1) {
		int bytesRecvd = recv(server, &buf, 1, 0);

		if (bytesRecvd < 0) {
			cerr << "Error: error when receiving server message" << endl;
			exit(4);
		} else if(bytesRecvd == 0) {
			break;
		}
		header += buf;
		if (header.size() >= 4) {
			string back = header.substr(header.size() - 4);
			if(back == "\r\n\r\n") {
				break;
			}
		}
	}
	return header;
}

string getInfo(string message, string info) {
	// int content_len = data.find("Content-Length:");
	int begin = message.find(info);

	int end = message.find('\n', begin);
	int space = message.rfind(' ', end);

	string result = message.substr(space + 1, end - space - 1);
	return result;
}

bool isChunk(string message) {
	auto seg = message.find("Seg");
	if(seg == string::npos) return false;
	auto frag = message.find("Frag", seg);
	if(frag == string::npos) return false;

	return true; 
}

pair<string, string> extractMessage(string message) {
	stringstream ss(message);
	string bin, temp;
	ss >> bin >> temp;
	int last_slash = temp.rfind('/');
	string file = temp.substr(last_slash + 1);
	string path = temp.substr(0, last_slash + 1);

	return make_pair(path, file);
}


int main(int argc, char  **argv)	//./miProxy <log> <alpha> <listen-port> <dns-ip> <dns-port> [<www-ip>]
{

	if(argc < 6){
		perror("not enough arguments");
	}

	char* log_path = argv[1];
	float alpha = atof(argv[2]);

	int port = atoi(argv[3]);		//convert string to integer
	
	char* dns_ip = argv[4];
	int dns_port = atoi(argv[5]);

	char* www_ip = "video.cse.umich.edu";

	if(argc > 6){
		www_ip = argv[6];
	}


	// int port = 3001;		//specify the port number for listener to bind

	int listener;

	listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(listener == -1){
		perror("Error occur at create socket!");
		return 1;
	}

	struct sockaddr_in addrProxy;

	addrProxy.sin_family = AF_INET;
	addrProxy.sin_port = htons((u_short) port);
	addrProxy.sin_addr.s_addr = INADDR_ANY;

	if(bind(listener, (struct sockaddr *)&addrProxy, sizeof(addrProxy)) == -1){
		close(listener);
		perror("server:bind");
		return 1;
	}
	if(listen(listener, 10) == -1){
		close(listener);
		perror("listen");
		return 1;
	}

	fd_set master;		//master file descriptor
	vector<int> read_fds;

	unordered_map<int, double> throughputs;

	unordered_map<int, string> server_ip;

	vector<int> available_bitrate;

	ofstream output(string(log_path), std::ofstream::out);

	while(1){

		FD_ZERO(&master);		//initialize the master and temporary sets
		FD_SET(listener, &master);
		for(int i = 0; i < (int)read_fds.size(); i++) {
			FD_SET(read_fds[i], &master);
		}

		int fdmax = 0;
		if(read_fds.size()) {
			fdmax = *max_element(read_fds.begin(), read_fds.end());
		}

		fdmax = max(fdmax, listener);


		if(select(fdmax + 1, &master, NULL, NULL, NULL) == -1){
			perror("something wrong while select");
			exit(4);
		}


		if(FD_ISSET(listener, &master)) {//handle new connection


			int clientsd = accept(listener, NULL, NULL);

			if(clientsd == -1) {
				perror("error on accept");
			} else {
				read_fds.push_back(clientsd);
			}

			cout << "new connection from" <<  clientsd << endl;
		}


		for(int i = 0; i <(int) read_fds.size(); i++){

			if(FD_ISSET(read_fds[i], &master)){

				int fd = read_fds[i];

				string request_str = "";
				char buf;
				bool closed = false;

				auto begin_time = chrono::high_resolution_clock::now();
				//receive from client
				while(1) {


					int bytesRecvd = recv(fd, &buf, 1, 0);

					if (bytesRecvd < 0) {
						cerr << "Error: error when receiving client message" << endl;
						exit(4);
					} else if(bytesRecvd == 0) {
						close(read_fds[i]);
						read_fds.erase(read_fds.begin() + i);
						if(throughputs.find(read_fds[i]) != throughputs.end()) {
							throughputs.erase(read_fds[i]);
						}
						closed = true;
						i-=1;
					}
					
					request_str += buf;
					if (request_str.size() >= 4) {
						string back = request_str.substr(request_str.size() - 4);
						if(back == "\r\n\r\n") {
							break;
						}
					}
				}

				if(!closed) {

					int curr_bitrate = 0;
					string reqested_chunkname = "";


					if(isChunk(request_str)) {
						int space = request_str.find("GET") + 4;
						int second = request_str.find(' ', space);
						int slash = request_str.rfind('/', second);
						int file_len = second - slash - 1;
						string filename = request_str.substr(slash + 1, file_len);

						reqested_chunkname = filename;
						
						int seg = filename.find("Seg");
						int default_bitrate = atoi(filename.substr(0, seg).c_str()); 
						curr_bitrate = default_bitrate;

						if(throughputs.find(fd) != throughputs.end()) {
							double t = throughputs[fd] / 1.5;
							int chosen = default_bitrate;
							for(auto x: available_bitrate) {
								if((double)x <= t) chosen = x;
								else break;
							}

							if(chosen != default_bitrate) {
								string newfilename = to_string(chosen) + filename.substr(seg);
								reqested_chunkname = newfilename;
								request_str.replace(slash + 1, file_len, newfilename);
							} 

							curr_bitrate = chosen;
						}

					}

					const char* server_addr;
					if(server_ip.find(fd) != server_ip.end()) {
						server_addr = server_ip[fd].c_str();
					} else {

						string temp = getServerIPfromDNS(dns_port, dns_ip, www_ip);
						if(temp == "") {

							server_addr = www_ip;
							server_ip[fd] = string(www_ip);

						} else {
							server_addr = temp.c_str();
							server_ip[fd] = temp;
						}



					}


					int server;
					struct sockaddr_in addrServer;
					if((server = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
						perror("opening TCP socket");
						return -1;
					}
					memset(&addrServer, 0, sizeof(addrServer));
					addrServer.sin_family = AF_INET;
					addrServer.sin_addr.s_addr = inet_addr(server_addr);
					addrServer.sin_port = htons((u_int)80);

					if(connect(server, (struct sockaddr *) &addrServer,
						 sizeof(addrServer)) < 0) {
						cout << "error connecting: cannot connect the server" << endl;
						return -1;
					}

					//send to server

					int bytesSent; 
					if((bytesSent = send(server, request_str.c_str(), (int) request_str.size(), 0)) < 0) {
						cout << "error sending to server " << endl;
						return -1;
					}

					// cout << "send to server over bytesSent: " << bytesSent << endl;

					//receive from server
					string data = receiveHeader(server);

					int length = atoi(getInfo(data, "Content-Length:").c_str());

					string content_type = getInfo(data, "Content-Type:");

					for(int k = 0; k < length; k++) {
						int bytesRecvd = recv(server, &buf, 1, 0);
						if (bytesRecvd <= 0) {
							cerr << "Error: error when receiving server message" << endl;
							exit(4);
						} 
						data += buf;
					}

					auto end_time = chrono::high_resolution_clock::now();
					auto dur = end_time - begin_time;
					auto ms = std::chrono::duration_cast<std::chrono::milliseconds> (dur).count();
					double seconds = ms / 1000.0;

					if(content_type.find("video/f4f") != string::npos ) {
						double t_new = (double) (length/1000) /seconds * 8;
						double t_curr = (throughputs.find(fd) == throughputs.end())?
							 available_bitrate[0] : throughputs[fd];
						t_curr =  alpha * t_new + (1 - alpha) * t_curr;
						throughputs[fd] = t_curr;

						output << seconds << " " << t_new << " " << t_curr << " " << curr_bitrate <<" "
							<< string(server_addr) << " " << reqested_chunkname << endl;

					} else if(content_type.find("text/xml") != string::npos) {
						int last = 0;
						size_t curr;
						while( (curr = data.find("bitrate", last)) != string::npos) {
							int first = data.find('\"', curr);
							int second = data.find('\"', first);
							int bitrate = atoi(data.substr(first + 1, second - first - 1).c_str());
							bool existed = false;
							for(auto x: available_bitrate) existed = (x == bitrate);
							if(!existed)	
								available_bitrate.push_back(bitrate);
							sort(available_bitrate.begin(), available_bitrate.end());
							last = second;
						}

						//request xxx_nolist.f4m

						int temp = request_str.find(".f4m");
						string new_request = request_str;
						new_request.replace(temp, 4, "_nolist.f4m");

						if(send(server, new_request.c_str(), (int)new_request.size(), 0) < 0) {
							cout << "error sending to server " << endl;
							return -1;
						}

						data = receiveHeader(server);

						int length = atoi(getInfo(data, "Content-Length:").c_str());
						for(int k = 0; k < length; k++) {
							int bytesRecvd = recv(server, &buf, 1, 0);
							if (bytesRecvd <= 0) {
								cerr << "Error: error when receiving server message" << endl;
								exit(4);
							} 
							data += buf;
						}

					}


					//send to client
					if(send(fd, data.c_str(), (int) data.size(), 0) < 0) {
						cout << "error sending to server " << endl;
						return -1;
					}

					// cout << "send to client over" << endl;

				}

								
	
			}
		}
	}

	exit(4);
	return 0;
}
