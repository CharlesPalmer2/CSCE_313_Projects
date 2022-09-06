#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>
#include <string>   // for constructor

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFORequestChannel.h"

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;


void patient_thread_function (BoundedBuffer* rb, int p, int n) {
    // for n request, produce a datamsg(pat_num, time, ECGNO) and push to request_buffer
    for(int i=0; i<n; i++){
    //      - time dependent on current request
    //      - at 0-> time = 0.000; at 1-> time = 0.004; at 2-> time = 0.008; ...
        byte_t buf[sizeof(datamsg)];
        datamsg x(p, (i*0.004), 1);	// data to find
        memcpy(buf, &x, sizeof(datamsg));
        (*rb).push(buf, sizeof(buf));
    }
}

void file_thread_function (BoundedBuffer* rb, string filename, FIFORequestChannel* chan, int m) {
    // file size
    filemsg fm(0, 0);
	int len = sizeof(filemsg) + (filename.size() + 1);
	char* buf2 = new char[len];
	memcpy(buf2, &fm, sizeof(filemsg));
	strcpy(buf2 + sizeof(filemsg), filename.c_str());
	chan->cwrite(buf2, len);  // Get file length
	__int64_t file_size = 0;
	chan->cread(&file_size, sizeof(__int64_t));
	// Calculate number of messages and final message length
	__int64_t num_fullmessage = floor(file_size / m);
	__int64_t num_final = file_size % m;
    // open output file; allocate the memory fseek; close the file
    FILE * file = fopen(("received/"+filename).c_str(), "wb");
    if (file == NULL){
        cout << " File Open Error\n";
        exit(0);
    }
    fseek(file, 0, SEEK_SET);
    fclose(file);
    // for num_fullmessage produce request for a full length m message
    __int64_t offset = 0;
    for (__int64_t i=0; i < num_fullmessage; i++){
        fm.offset = offset;
		fm.length = m;
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), filename.c_str());
        (*rb).push(buf2, len);
        offset += m;
    }
    // Final data message (message of length less than m) if needed
    if(num_final != 0){
        fm.offset = offset;
		fm.length = num_final;
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), filename.c_str());
        (*rb).push(buf2, len);
    }
    delete[] buf2;
}

void worker_thread_function (BoundedBuffer* rb, BoundedBuffer* resb, FIFORequestChannel* chan, int m) {
    // forever loop
    while(true){
        // pop message from the request_buffer
        char* msg = new char[m];
        (*rb).pop(msg, m);
        // view line 120 in server (process_request function) for how to decide current message
        if(*((MESSAGE_TYPE*)msg) == QUIT_MSG){
            delete[] msg;
            break;
        }
        // if DATA:
        if(*((MESSAGE_TYPE*)msg) == DATA_MSG){
            //      - send the message across the FIFO channel and collect response
            double reply;
            chan->cwrite(msg, sizeof(datamsg));
            chan->cread(&reply, sizeof(double));
            //      - create pair of pat_num from message and response from server
            datamsg* dm = (datamsg*) msg;
            pair<int, double> point = make_pair(dm->person, reply);
            //      - push that pair to response_buffer
            (*resb).push((char*)&point, sizeof(point));
        }
        // if FILE:
        if(*((MESSAGE_TYPE*)msg) == FILE_MSG){
            //      - collect the filename from the message
            filemsg* fm = (filemsg*) msg;
            string filename = (char*) (fm+1);
            int len = sizeof(filemsg) + (filename.size() + 1);
            //      - send the message across the FIFO channel and collect response
            char* buf3 = new char[m+1];
            chan->cwrite(msg, len);
            chan->cread(buf3, m+1);
            //      - open the file in update mode, fseek(SEEK_SET) to offset of the filemsg
            FILE * file;
            file = fopen(("received/"+filename).c_str(), "r+");
            fseek(file, fm->offset , SEEK_SET);
            //      - write the buffer from the server
            fwrite(buf3, 1, fm->length, file);
            fclose(file);
            delete[] buf3;
        }
        delete[] msg;
    }
}

void histogram_thread_function (BoundedBuffer* resb, HistogramCollection* hc, int m) {
    // functionality of the histogram threads

    // forever loop
    while(true) {
        // pop response from the response_buffer
        char* msg = new char[m];
        (*resb).pop(msg, m);
        pair<int, double>* point = (pair<int, double>*) msg;
        if(point->first == 0){
            delete[] msg;
            break;
        }
        // call HC::update(resp->pat_num, resp->double)
        hc->update(point->first, point->second);
        delete[] msg;
    }
}


int main (int argc, char* argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed)
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
		}
	}
    
	// fork and exec the server
    int pid = fork();
    if (pid == 0) {
        execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    }
    
	// initialize overhead (including the control channel)
	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    FIFORequestChannel* f_chan; // For file thread
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    // vector of producer(patient) threads (if data, p elements; if file, 1 element)
    vector<thread> patient_threads;
    // vector of FIFO's (w elements)
    vector<FIFORequestChannel*> FIFO;
    // vector of worker threads (w elements)
    vector<thread> worker_threads;
    // vector of histogram threads (if data, h elements; if file, 0 elements)
    vector<thread> hist_threads;

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* create all threads here */
    // if data:
    if (f == ""){
        //      - create p patient_threads
        for (int i=0; i<p; i++){
            patient_threads.push_back(thread(patient_thread_function, &request_buffer, (i+1), n));
        }
    }
    // if file:
    if (f != ""){
        //      - create 1 file_thread (store producer array)
        char name[30];  // 30 is the size of buffer in server
        MESSAGE_TYPE new_mes = NEWCHANNEL_MSG;
        chan->cwrite(&new_mes, sizeof(new_mes));
        chan->cread(name, sizeof(name));
        f_chan = new FIFORequestChannel(name, FIFORequestChannel::CLIENT_SIDE);
        patient_threads.push_back(thread(file_thread_function, &request_buffer, f, f_chan, m));
    }
    // create w worker_threads (store worker array)
    for (int i=0; i<w; i++){
        //      - create w channel (store FIFO array)
        char name[30];  // 30 is the size of buffer in server
        MESSAGE_TYPE new_mes = NEWCHANNEL_MSG;
        chan->cwrite(&new_mes, sizeof(new_mes));
        chan->cread(name, sizeof(name));
        FIFO.push_back(new FIFORequestChannel(name, FIFORequestChannel::CLIENT_SIDE));
        worker_threads.push_back(thread(worker_thread_function, &request_buffer, &response_buffer, FIFO.at(i), m));
    }
    // if data:
    if (f == ""){
        //      - creat h histogram_threads (store hist array)
        for(int i=0; i<h; i++){
            hist_threads.push_back(thread(histogram_thread_function, &response_buffer, &hc, m));
        }
    }

	/* join all threads here */
    // iterate over all thread arrays, calling join
    //      - order (producer, worker, histogram)
    for (int i=0; i<(int)patient_threads.size() ;i++){
        patient_threads.at(i).join();
    }
    // send QUIT_MSG's for each worker thread
    MESSAGE_TYPE quit = QUIT_MSG;
        for (int i=0; i<w ;i++){
        char qb[sizeof(MESSAGE_TYPE)];
        memcpy(qb, &quit, sizeof(MESSAGE_TYPE));
        request_buffer.push(qb, sizeof(qb));
    }
    // Wait for worker threads to join
    for (int i=0; i<w ;i++){
        worker_threads.at(i).join();
    }
    // Send exit code to histogram threads
    for (int i=0; i<(int)hist_threads.size() ;i++){
        pair<int, double> point = make_pair(0, 0);
        response_buffer.push((char*)&point, sizeof(point));
    }
    // Wait for histogram threads to join
    for (int i=0; i<(int)hist_threads.size() ;i++){
        hist_threads.at(i).join();
    }

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    // quit and close all channels in FIFO array
    for (int i=0; i<w; i++){
        FIFORequestChannel* FIFO_ptr = FIFO.at(i);
        (*FIFO_ptr).cwrite((char *)&quit, sizeof(MESSAGE_TYPE));
        delete FIFO.at(i);
    }
    if (f != ""){
        f_chan->cwrite ((char *) &quit, sizeof (MESSAGE_TYPE));
        delete f_chan;
    }
	// quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;

	// wait for server to exit
	wait(nullptr);
}
