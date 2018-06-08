#include "miProxy.h"


#define SIZE 16000
#define PORT_APACHE 80
#define ADDR "127.0.0.1"

using namespace std;
void miProxy::pttoLog(string filePath,float duration,float tput,float avgTput,int bitrate,string serverIp,string chunkName){
	ofstream fout;
	fout.open(filePath,ios_base::out|ios_base::app);
	fout<<duration<<" "<<tput<<" "<<avgTput<<" "<<bitrate<<" "<<serverIp<<" "<<chunkName<<endl;
	fout<<flush;
	fout.close();
}
miProxy::miProxy(char* logName,float alp,int por,char* ip){
	toLog=string(logName);
	alpha=alp;
	port=por;
	wwwip=string(ip);
	sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //listen to browser
    	ser = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//connect to the server

	self.sin_family = AF_INET;
	self.sin_addr.s_addr = INADDR_ANY;
	self.sin_port = htons(port);

    	seraddr.sin_family = AF_INET;
    	seraddr.sin_addr.s_addr = inet_addr(wwwip.c_str());
    	seraddr.sin_port = htons(PORT_APACHE);

	int err = bind(sd, (struct sockaddr*) &self, sizeof(self));
	if(err == -1)
	{
		cout << "Error binding the socket to the port number\n";
		exit(1);
	}

	err = listen(sd, 10);
	if(err == -1)
	{
		cout << "Error setting up listen queue\n";
		exit(1);
	}
}



int miProxy::timeval_subtract (struct timeval * result, struct timeval * x, struct timeval * y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

vector<int> miProxy::getBitrates(const std::string& xml) {
	using std::string;
	string keyword = "bitrate=\"";
	vector<int> lst;
	for(size_t tagPos = xml.find("<media"); tagPos != string::npos; tagPos = xml.find("<media", tagPos + 1)) {
		size_t keyPos = xml.find(keyword, tagPos);
		if(keyPos == string::npos)
			continue;
		int bitrateLoc = keyPos + keyword.size();
		int len = xml.find('"', bitrateLoc) - bitrateLoc;
		int bitrate = std::stoi(xml.substr(bitrateLoc, len));
		lst.push_back(bitrate);
	}
	return lst;
}

string miProxy::recvBuffer(int ser, int &cont_length){//read the whole chunk
    std::string buffer("");
    std::string deli ("\r\n\r\n");
    while (1){
        char buf = 'x';
        if (recv(ser, &buf, 1, 0) > 0){
            buffer += buf;
        } else{
            std::cout << "Error recving bytes" << std::endl;
            exit(1);
        }

        std::size_t found = buffer.find(deli);
        if (found != std::string::npos){
            break;
        }
    }

    deli = "Content-Length: ";
    std::size_t found = buffer.find(deli);
    cont_length = 0;
    //std::cout << left << " " << buffer.size() <<" "<<buffer.substr(left) << std::endl;
    if (found != std::string::npos){
        unsigned left = buffer.find(deli);
        unsigned right = buffer.find("Keep-Alive: ");

        cont_length = std::stoi(buffer.substr(left+16, right-left));

        int cnt = 0;
        while (cnt < cont_length){
            char buf = 'x';
            if (recv(ser, &buf, 1, 0) > 0){
                buffer += buf;
                cnt ++;
            } else{
                exit(1);
            }
        }

    }

    return buffer;
}

void miProxy::run(){
	int count = 0;
	float tput=0;
	float curTput=0;
	int firstF4m=0;
	bool isChunk=false;
	while(true)
	{
        	int xml = 0;
        	//std::cout << count << std::endl;
		// Set up the readSet
		FD_ZERO(&readSet);
		FD_SET(sd, &readSet);
		for(int i = 0; i < (int) fds.size(); ++i)
		{
			FD_SET(fds[i], &readSet);
		}


		int maxfd = 0;
		if(fds.size() > 0)
		{
			maxfd = *std::max_element(fds.begin(), fds.end());
		}
		maxfd = std::max(maxfd, sd);

		// maxfd + 1 is important
		int err = select(maxfd + 1, &readSet, NULL, NULL, NULL);
		assert(err != -1);

		if(FD_ISSET(sd, &readSet))
		{
			int clientsd = accept(sd, NULL, NULL);
			if(clientsd == -1)
			{
				std::cout << "Error on accept" << std::endl;
			}
			else
			{
				fds.push_back(clientsd);
			}
		}

		for(int i = 0; i < (int) fds.size(); ++i)
		{

			if(FD_ISSET(fds[i], &readSet))
			{
                		std::string buffer ("");
                		char buf[SIZE] = {0};
                		struct timeval before, after, tresult;

                		
				int bytesRecvd = recv(fds[i], &buf, SIZE, 0);
                                gettimeofday(&before, NULL);
				if(bytesRecvd < 0)
				{
					std::cout << "Error recving bytes" << std::endl;
					//std::cout << strerror(errno) << std::endl;
					exit(1);
				}
				else if(bytesRecvd == 0)
				{
					std::cout << "Connection closed" << std::endl;
					fds.erase(fds.begin() + i);
				}else{
                    		//printf("Received from client: %s\n", buf);
                		}
                		buffer += buf;
                		std::string bunny = ".f4m";
                		std::size_t bun = buffer.find(bunny);
				
				std::size_t chunk=buffer.find("-Frag");
				
                		connect(ser,(struct sockaddr*)&seraddr, sizeof(seraddr));//sometimes it may fail to connect to server,if so, just skip it.
	
				
                		int cont_length = 0;

                		if (bun != std::string::npos){
                    			send(ser, buffer.c_str(), buffer.size(), 0);
                    			bitrates = getBitrates(recvBuffer(ser, cont_length));
                    			buffer.insert(bun, "_nolist");
                    			xml = 1;
					isChunk=false;
                    			std::cout<<"----"<<buffer<<std::endl;
                		}else if (chunk != std::string::npos){
					isChunk=true;
					xml = 0;			
				}else{
					isChunk=false;
					xml=0;				
				}
	
				if (xml){
                    			//bitrates = getBitrates(buffer);
                    			std::cout<<buffer<<std::endl;
                    			std::cout<<"Bitrates: "<<std::endl;
					int minb=60000;
                    			for (int i = 0; i < bitrates.size(); i++){
						minb=std::min(minb,bitrates[i]);
                        			std::cout<<bitrates[i]<<std::endl;
                    			}
					if(firstF4m==0){
						firstF4m=1;
						tput=minb;
						curTput=minb;					
					}
                		}
				string newBitrate="";
				if(isChunk){
					std::size_t toModify1=buffer.find("vod/");
					std::size_t toModify2=buffer.find("Seg");
					float limit=(float)tput*2.0/3.0;
					int maxBitrate=0;
					for (int i = 0; i < bitrates.size(); i++){
						if(bitrates[i]<=limit){
							maxBitrate=std::max(maxBitrate,bitrates[i]);					
						}						
                    			}
					if(maxBitrate==0){
						maxBitrate=tput;
						cout<<"firstbitrate: "<<maxBitrate<<endl;
					}
						
					newBitrate=std::to_string(maxBitrate);
					buffer.replace(toModify1+4,toModify2-toModify1-4,newBitrate);					
				}
				std::size_t nameStart=buffer.find("http");
				std::size_t nameEnd=buffer.find("HTTP");
				string name=buffer.substr(nameStart,nameEnd-nameStart-1);
                		
                		int bytesSent = send(ser, buffer.c_str(), buffer.size(), 0);
                		
                		if (bytesSent <= 0){
                    			std::cout << "Error sending bytes" << std::endl;
                    			exit(1);
                		}

                		
                    		
                		//std::cout << " Sent to server: "<< buffer <<"----"<< std::endl;

                		buffer = "";
                		std::string deli ("\r\n\r\n");
                		//printf("entered\n");


                		buffer = recvBuffer(ser, cont_length); // receive data from server
                	

                		gettimeofday(&after, NULL);

                		timeval_subtract ( &tresult, &after, &before );

                		
				

                		
                		//std::cout << "Throughput: "<<cont_length<<", "<<elapsed<<", " << throughput << std::endl;
				                             
				if(isChunk){					
					double elapsed = tresult.tv_sec + (tresult.tv_usec*0.000001);
                			curTput = cont_length*8 / elapsed / 1000;  //Kbps
					tput=alpha*curTput+(1-alpha)*tput;
					pttoLog(toLog,elapsed,curTput,tput,std::stoi(newBitrate),wwwip,name);				
				}
				
                		// if (count == 5){
                		//     std::cout <<cont_length << " Sent to client: "<< buffer <<"----"<< std::endl;
                		// }
                		send(fds[i], buffer.c_str(), buffer.size(), 0);
                		count++;

			}
		}
	}

}
int main (int argc, char* argv[]){

    	if (argc != 5){
        	fprintf(stderr, "Usage: ./miProxy <log> <alpha> <listen-port> <www-ip>\n");
        	return 1;
    	}
	miProxy myProxy(argv[1],atof(argv[2]),atoi(argv[3]),argv[4]);
	myProxy.run();
    	return 0;

}
