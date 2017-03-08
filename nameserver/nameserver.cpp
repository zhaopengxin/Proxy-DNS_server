#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>

#include <fstream>
#include <sstream>

#include <vector>

#include <unordered_map>
#include <unordered_set>

//arpa/inet.h for INADDR_ANY
#include "../starter_code/DNSHeader.h"
#include "../starter_code/DNSQuestion.h"
#include "../starter_code/DNSRecord.h"

using namespace std;

char buf;			//using for receive data
int count = 0;		//using for count num of query

/*
	this is part for distance round robin, distance based is below
*/
string receive_request(int newRequest, char buf){
	string dns_str;
	while(true){
		int byteReceived = recv(newRequest, &buf, 1, 0);
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
	return dns_str;
}

void send_back_header(int sock, int good){
	DNSHeader dnsheader;
	dnsheader.ID = 1;
	dnsheader.QR = 1;
	dnsheader.OPCODE = 1;
	dnsheader.AA = 0;
	dnsheader.TC = 1;
	dnsheader.RD = 0;
	dnsheader.RA = 0;
	dnsheader.Z = '1';
	if(good == 0){
		dnsheader.RCODE = '3';
	}else{
		dnsheader.RCODE = '0';
	}
	dnsheader.QDCOUNT = 1;
	dnsheader.ANCOUNT = 1;
	dnsheader.NSCOUNT = 1;
	dnsheader.ARCOUNT = 1;
	char buff[sizeof(dnsheader)];
	memcpy(buff, &dnsheader, sizeof(dnsheader));
	send(sock, buff, sizeof(buff), 0);

	char break_point = 0x4;
	send(sock, &break_point, 1, 0);
}

void send_back_good_request(int newRequest, string ipString){
	DNSRecord dns_anwser;
	strncpy(dns_anwser.NAME, ipString.c_str(), sizeof(dns_anwser.NAME));
	dns_anwser.TYPE = 1;	//this always set to 1
	dns_anwser.CLASS = 1;	//this always set to 1
	dns_anwser.TTL = 1;	//this always set to 1	
	char *rdata = "11";
	strcpy(dns_anwser.RDATA, rdata);//with no idea!!!!!!!
	dns_anwser.RDLENGTH = sizeof(dns_anwser.RDATA);

	char buff[sizeof(dns_anwser)];
	memcpy(buff, &dns_anwser, sizeof(dns_anwser));
	send(newRequest, buff, sizeof(buff), 0);

	char break_point = 0x4;
	send(newRequest, &break_point, 1, 0);
}

void roundRobin(char *log_path, int port, string servers){
	ofstream output(string(log_path), std::ofstream::out);
	vector<string> ipAddr;
	ifstream ifs;
	ifs.open(servers);
	string temp;
	getline(ifs, temp);
	while(temp != ""){
		stringstream ss(temp);
		ss >> temp;
		ipAddr.push_back(temp);
		getline(ifs, temp);
	}

	int dnsServer = socket(AF_INET, SOCK_STREAM, 0);
	if(dnsServer == -1){
		perror("Error occur at create socket!");
		return;
	}
	struct sockaddr_in addrDNS;
	struct sockaddr_in addrProxy;
	addrDNS.sin_family = AF_INET;
	addrDNS.sin_port = port;
	addrDNS.sin_addr.s_addr = INADDR_ANY;
	if(bind(dnsServer, (struct sockaddr *)&addrDNS, sizeof(addrDNS)) == -1){
		close(dnsServer);
		perror("server:bind");
		return;
	}

	if(listen(dnsServer, 10) == -1){
		close(dnsServer);
		perror("listen");
		return;
	}
	socklen_t addr_size = sizeof(addrProxy);

	//in a while loop for continue listen
	while(true){
		int newRequest = accept(dnsServer, (struct sockaddr *)&addrProxy, &addr_size);
		if(newRequest == -1){
			close(dnsServer);
			perror("accept");
			return;
		}

		//parse the client ip
		string client_ip(inet_ntoa(addrProxy.sin_addr));

		//receive dns_request
		string dns_request = receive_request(newRequest, buf);
		DNSHeader dnsHeader;
		memcpy(&dnsHeader, dns_request.c_str(), dns_request.size());
		if(dnsHeader.QR == 0){
			string dns_question = receive_request(newRequest, buf);
			DNSQuestion dnsQuestion;
			memcpy(&dnsQuestion, dns_question.c_str(), dns_question.size());
			char qname[100] = "video.cse.umich.edu";
			// cout<<"domain_name: "<<dnsQuestion.QNAME<<endl;
			if(strcmp(qname, dnsQuestion.QNAME) != 0){
				// cout<<"send bad request"<<endl;
				output << client_ip << " " << dnsQuestion.QNAME << " " << " " << endl;
				send_back_header(newRequest, 0);
			}else{
				// cout<<"send good request"<<endl;
				int seq_of_server = count%ipAddr.size();
				count++;
				send_back_header(newRequest, 1);
				string ipString = ipAddr[seq_of_server];
				output << client_ip << " " << dnsQuestion.QNAME << " " << ipString << endl;
				send_back_good_request(newRequest, ipString);
			}
		}
	}
	close(dnsServer);
}


/*
	this is part for distance based search
*/

struct Node {
	string type;
	string ip;
	int id;

	unordered_map<Node*, int> adjacent;

	Node(string wholeline) {
		int first = wholeline.find(' ');
		int second = wholeline.find(' ', first + 1);

		id = atoi(wholeline.substr(0, first).c_str());
		type = wholeline.substr(first + 1, second - first - 1);
		ip = wholeline.substr(second + 1);

	}
};

struct Edge {
	int small;
	int large;
	int length;
	Edge(string temp) {
		int first = temp.find(' ');
		int second = temp.find(' ', first + 1);
		int edge_a = atoi(temp.substr(0, first).c_str());
		int edge_b = atoi(temp.substr(first + 1, second - first - 1).c_str());
		length = atoi(temp.substr(second + 1).c_str());
		small = min(edge_a, edge_b);
		large = max(edge_a, edge_b);

	}
};


unordered_map<string, string> buildShortestPathMap(vector<Node*> client_nodes,
	 vector<Node*> server_nodes) {
	// Dijkstra's Algorithm

	unordered_map<string, string> result;

	for(auto client: client_nodes) {

		unordered_set<Node*> known;
		unordered_set<Node*> collected;
		unordered_map<Node*, int> distance;
		unordered_map<Node*, Node*> parent_node;

		parent_node[client] = nullptr;

		distance[client] = 0;
		collected.insert(client);

		while(!collected.empty()) {
			Node* next = *collected.begin();
			auto set_it = collected.begin();
			auto to_remove = set_it;
			while(set_it != collected.end()) {
				auto node = *set_it;
				to_remove = (distance[node] >= distance[next])? to_remove: set_it;
				next = (distance[node] >= distance[next])? next : node;
				set_it++;
			}

			auto it = next->adjacent.begin();
			while(it != next->adjacent.end()) {
				Node* node = it->first;
				int dist = it->second + distance[next];
				if(node -> type != "CLIENT") {
					if(distance.find(node) == distance.end()) {
						distance[node] = dist;
						parent_node[node] = next;
						collected.insert(node);
					} else {
						if(dist < distance[node]) {
							distance[node] = dist;
							parent_node[node] = next;
						}
					}
				}
				it++;
			}
			if(next -> type == "SERVER")
				known.insert(next);


			collected.erase(next);
		}

		Node* closest = *known.begin();
		auto itt = known.begin();
		while(itt != known.end()) {
			closest = (distance[*itt] < distance[closest] )? *itt: closest;
			itt++;
		}

		result[client->ip] = closest->ip;

		// cout << client->ip << ": " << closest->ip << endl;

	}
	return result;
}


void distance_based(char *log_path, int port, string servers){
	/*
		create your read file as needed
		log path, port, servers are parameter

	*/
	ofstream output(string(log_path), std::ofstream::out);
	ifstream ifs;
	ifs.open(servers, std::ifstream::in);

	string temp;
	getline(ifs, temp);
	stringstream ss(temp);
	ss >> temp;
	int line;
	ss >> line; 

	unordered_map<int, Node*> allnodes;

	vector<Node*> client_nodes;
	vector<Node*> server_nodes;

	for(int i = 0; i < line; i++) {
		getline(ifs, temp);
		Node* next = new Node(temp);
		allnodes[next->id] = next;
		if(next -> type == "SERVER") server_nodes.push_back(next);
		else if(next -> type == "CLIENT") client_nodes.push_back(next);
	}

	vector<Edge*> edges;

	getline(ifs, temp);
	int space = temp.find(' ');
	line = atoi(temp.substr(space + 1).c_str());
	for(int i = 0; i < line; i++) {
		getline(ifs, temp);
		Edge* e = new Edge(temp);
		edges.push_back(e);
	}

	for(auto e: edges) {
		allnodes[e->small] -> adjacent[allnodes[e->large]] = e->length;
		allnodes[e->large] -> adjacent[allnodes[e->small]] = e->length;
	}

	unordered_map<string, string> closest = buildShortestPathMap(client_nodes, server_nodes);

	

	//create socket, it is the same as above
	int dnsServer = socket(AF_INET, SOCK_STREAM, 0);
	if(dnsServer == -1){
		perror("Error occur at create socket!");
		return;
	}
	struct sockaddr_in addrDNS;
	struct sockaddr_in addrProxy;
	addrDNS.sin_family = AF_INET;
	addrDNS.sin_port = port;
	addrDNS.sin_addr.s_addr = INADDR_ANY;
	if(bind(dnsServer, (struct sockaddr *)&addrDNS, sizeof(addrDNS)) == -1){
		close(dnsServer);
		perror("server:bind");
		return;
	}

	if(listen(dnsServer, 10) == -1){
		close(dnsServer);
		perror("listen");
		return;
	}
	socklen_t addr_size = sizeof(addrProxy);

	//in a while loop for continue listen
	while(true){
		int newRequest = accept(dnsServer, (struct sockaddr *)&addrProxy, &addr_size);

		if(newRequest == -1){
			close(dnsServer);
			perror("accept");
			return;
		}
		//parse the client ip
		string client_ip(inet_ntoa(addrProxy.sin_addr));

		//receive dns_request
		string dns_request = receive_request(newRequest, buf);
		DNSHeader dnsHeader;
		memcpy(&dnsHeader, dns_request.c_str(), dns_request.size());
		if(dnsHeader.QR == 0){
			string dns_question = receive_request(newRequest, buf);
			DNSQuestion dnsQuestion;
			memcpy(&dnsQuestion, dns_question.c_str(), dns_question.size());
			char qname[100] = "video.cse.umich.edu";
			// cout<<"domain_name: "<<dnsQuestion.QNAME<<endl;
			if(strcmp(qname, dnsQuestion.QNAME) != 0){
				// cout<<"send bad request"<<endl;
				output << client_ip << " " << dnsQuestion.QNAME << " " << " " << endl;
				send_back_header(newRequest, 0);
			}else{
				// cout<<"send good request"<<endl;
				send_back_header(newRequest, 1);

				// cout << client_ip << endl;

				string ipString = closest[client_ip];

				// cout <<"resonse" << ipString << endl;
				output << client_ip << " " << dnsQuestion.QNAME << " " << ipString << endl;
				send_back_good_request(newRequest, ipString);
			}
		}
	}
	close(dnsServer);
}

//this is main function
int main(int argc, char **argv){

	char* log_path = argv[1];
	int port = atoi(argv[2]);		
	int geography_based = atoi(argv[3]); //0 : round-robin, 1: ditance based
	string servers(argv[4]);
	
	if(geography_based == 0){
		roundRobin(log_path, port, servers);
	}else if(geography_based == 1){
		distance_based(log_path, port, servers);
	}else{
		printf("Error");
		return 1;
	}
}

