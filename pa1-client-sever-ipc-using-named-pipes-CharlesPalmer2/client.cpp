/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Charles Palmer
	UIN: 427003092
	Date: 6/11/2022
*/
#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;

int main (int argc, char *argv[]) {

	int opt;
	int p = 0;
	double t = 0.0;
	int e = 0;
	__int64_t m = MAX_MESSAGE;
	bool c = false;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi (optarg);
				break;
			case 'c':
				c = true;
				break;
		}
	}

	// Run server as child
	if (fork()==0){ // if child
		// Run server.cpp
		char *args[] = {(char*)"./server", (char*)"-m", (char*)(to_string(m).c_str()), nullptr};
		execvp(args[0], args);
	}
	// Else, continue as client
	
	FIFORequestChannel* cont_chann;
    FIFORequestChannel* chann = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
	
	// Request new channel
	if (c == true){
		MESSAGE_TYPE new_chan = NEWCHANNEL_MSG;
		chann->cwrite(&new_chan, sizeof(MESSAGE_TYPE));	// Get new channel name
		char buf[30];	// 30 is the size of buffer in server
		chann->cread(buf, sizeof(string));
		string chan_name = buf;
		// Connect to new channel
		cont_chann = chann;
		chann = new FIFORequestChannel(chan_name, FIFORequestChannel::CLIENT_SIDE);
	}

	// Data point request
	if (p != 0 && e != 0){	// Singular data point
		byte_t buff[MAX_MESSAGE];
		datamsg x(p, t, e);	// data to find

		memcpy(buff, &x, sizeof(datamsg));
		chann->cwrite(buff, sizeof(datamsg));	// question
		double reply;
		chann->cread(&reply, sizeof(datamsg));	// answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}
	else if (p != 0){	// Multiple data points
		// Create file
		ofstream file("./received/x1.csv");
		char buff[MAX_MESSAGE];
		double reply;
		// Add data points to file
		for (int i = 0; i < 1000; i++){	// First 1000 data points
			datamsg x(p, t, 1);	// Data to find for ecg 1
			memcpy(buff, &x, sizeof(datamsg));
			chann->cwrite(buff, sizeof(datamsg));	// question
			chann->cread(&reply, sizeof(datamsg));	// answer
			file << t << "," << reply << ",";
			datamsg y(p, t, 2);	// Data to find for ecg 2
			memcpy(buff, &y, sizeof(datamsg));
			chann->cwrite(buff, sizeof(datamsg));	// question
			chann->cread(&reply, sizeof(datamsg));	// answer
			file << reply << "\n";
			t += 0.004;
		}
		// close file
		file.close();
	}
	else if (filename != ""){	// File request
		filemsg fm(0, 0);
		int len = sizeof(filemsg) + (filename.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), filename.c_str());
		chann->cwrite(buf2, len);  // Get file length
		__int64_t file_size = 0;
		chann->cread(&file_size, sizeof(__int64_t));
		// Calculate number of messages and final message length
		__int64_t num_fullmessage = floor(file_size / m);
		__int64_t num_final = file_size % m;
		// Set up file
		ofstream file("received/"+filename, ios:: trunc | ios::in | ios::binary);

		// Request data messages
		__int64_t offset = 0;
		for (__int64_t i=0; i < num_fullmessage; i++){
			fm.offset = offset;
			fm.length = m;
			memcpy(buf2, &fm, sizeof(filemsg));
			strcpy(buf2 + sizeof(filemsg), filename.c_str());
			chann->cwrite(buf2, len);	// get message
			char* buf3 = new char[m+1];
			chann->cread(buf3, m+1);

			// Load message into file and loop
			file.write(buf3, m);
			offset += m;
			delete[] buf3;
		}
		// Final data message
		if(num_final != 0){
			fm.offset = offset;
			fm.length = num_final;
			memcpy(buf2, &fm, sizeof(filemsg));
			strcpy(buf2 + sizeof(filemsg), filename.c_str());
			chann->cwrite(buf2, len);	// get message
			char* buf4 = new char[(num_final)+1];
			chann->cread(buf4, (num_final)+1);
			file.write(buf4, (num_final));
			delete[] buf4;
		}

		file.close();
		delete[] buf2;
	}
	// closing the channel    
    MESSAGE_TYPE quit = QUIT_MSG;
    chann->cwrite(&quit, sizeof(MESSAGE_TYPE));
	delete chann;
	if(c==true){
		MESSAGE_TYPE quit = QUIT_MSG;
    	cont_chann->cwrite(&quit, sizeof(MESSAGE_TYPE));
		delete cont_chann;
	}


	// example data point request
    //char buf[MAX_MESSAGE]; // 256
    //datamsg x(1, 0.0, 1);
	//
	//memcpy(buf, &x, sizeof(datamsg));
	//chan.cwrite(buf, sizeof(datamsg)); // question
	//double reply;
	//chan.cread(&reply, sizeof(double)); //answer
	//cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	//
    // sending a non-sense message, you need to change this
	//filemsg fm(0, 0);
	//string fname = "teslkansdlkjflasjdf.dat";
	//
	//int len = sizeof(filemsg) + (fname.size() + 1);
	//char* buf2 = new char[len];
	//memcpy(buf2, &fm, sizeof(filemsg));
	//strcpy(buf2 + sizeof(filemsg), fname.c_str());
	//chan.cwrite(buf2, len);  // I want the file length;
	//
	//delete[] buf2;
	//
	// closing the channel    
    //MESSAGE_TYPE m = QUIT_MSG;
    //chan.cwrite(&m, sizeof(MESSAGE_TYPE));
}
