#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <chrono>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

using namespace std;

struct fi_info *info;
struct fid_fabric *fabric;
struct fid_domain *domain;
struct fid_ep *ep;
struct fid_cq *cq;
struct fid_mr *mr;
struct fid_av *av;

fi_addr_t fi_addr;

//const size_t buffer_size = 1024*1024*1024;
//const size_t buffer_size = 8*1024;
const size_t buffer_size = 1024 * 1024;
//const size_t buffer_size = 16*1024;
//const size_t buffer_size = 1024;
const unsigned int iterations = 2000000;

struct mr_message {
	uint64_t mr_key;
	uintptr_t addr;
} mr_message;

struct fi_custom_context {
	struct fi_context context;
	uint64_t id;
	uint64_t op_context;
} fi_custom_context;

char *buffer;
struct mr_message recv_buffer;
struct mr_message send_buffer;
struct fi_custom_context fi_recv_context;
struct fi_custom_context fi_send_context;
struct fi_custom_context fi_write1_context;
struct fi_custom_context fi_write2_context;
uint64_t remote_key;
uintptr_t remote_addr;
uint64_t mr_key;
unsigned int counter = 0;
bool dest_terminated = false;
std::chrono::time_point<std::chrono::high_resolution_clock> startT, endT;

char *ip_address, *alps_app_pe;

char* get_other_address() {
	std::string line;

	ifstream file("nodes");
	if (!file.is_open())
		cout << "[pid:" << alps_app_pe << "]error while opening file" << endl;

	while (getline(file, line)) {
		cout << "[pid:" << alps_app_pe << "]read from nodes file: " << line.c_str() << " local is "
				<< ip_address << endl;
		if (ip_address != line)
			return strdup(line.c_str());
	}
	return strdup("");
}

struct fi_info* get_hints() {
	struct fi_info *hints = fi_allocinfo();

	hints->caps = FI_MSG | FI_RMA | FI_WRITE | FI_SEND | FI_RECV
			| FI_REMOTE_WRITE | FI_TAGGED;
	hints->mode = FI_LOCAL_MR | FI_CONTEXT;
	hints->ep_attr->type = FI_EP_RDM;
	hints->domain_attr->threading = FI_THREAD_COMPLETION;
	hints->domain_attr->data_progress = FI_PROGRESS_AUTO;
	hints->domain_attr->mr_mode = FI_MR_BASIC;
	hints->fabric_attr->prov_name = strdup("psm2");

	return hints;
}
struct fi_info* find_psm2() {
	struct fi_info *hints = get_hints();

	int res = fi_getinfo(FI_VERSION(1, 5), ip_address, "14195", FI_SOURCE,
			hints, &info);
	assert(res == 0);
	assert(info != NULL);

	while (info != NULL) {
		if (strcmp("psm2", info->fabric_attr->prov_name) == 0)
			return info;
		info = info->next;
	}

	assert(info != NULL);

	return NULL;
}

struct fi_info* find_other_addr() {
	struct fi_info *info = fi_allocinfo();
	struct fi_info *hints = get_hints();
	char *other_addr = get_other_address();
	cout << "[pid:" << alps_app_pe << "] other_addr: " << other_addr << endl;

	int res = fi_getinfo(FI_VERSION(1, 5), other_addr, "14195", FI_NUMERICHOST,
			hints, &info);
	assert(res == 0);
	assert(info != NULL);

	while (info != NULL) {
		if (strcmp("psm2", info->fabric_attr->prov_name) == 0)
			return info;
		info = info->next;
	}

	assert(info != NULL);

	return NULL;
}

void init_fabric_domain() {
	// fabric
	cout << "[pid:" << alps_app_pe << "]setup fabric" << endl;
	int res = fi_fabric(info->fabric_attr, &fabric, NULL);
	assert(res == 0);

	// domain
	cout << "[pid:" << alps_app_pe << "]setup domain" << endl;
	res = fi_domain(fabric, info, &domain, NULL);
	assert(res == 0);
}

void init_cq() {
	// cq
	cout << "[pid:" << alps_app_pe << "]setup cq" << endl;
	struct fi_cq_attr cq_attr;
	memset(&cq_attr, 0, sizeof(cq_attr));
	cq_attr.size = 1000000;
	cq_attr.flags = 0;
	//cq_attr.format = FI_CQ_FORMAT_CONTEXT;
	cq_attr.format = FI_CQ_FORMAT_TAGGED;
	cq_attr.wait_obj = FI_WAIT_NONE;
	cq_attr.signaling_vector = 1; // ??
	cq_attr.wait_cond = FI_CQ_COND_NONE;
	cq_attr.wait_set = nullptr;
	int res = fi_cq_open(domain, &cq_attr, &cq, NULL);
	assert(res == 0);
}

void init_ep() {
	cout << "[pid:" << alps_app_pe << "]create endpoint" << endl;
	int res = fi_endpoint(domain, info, &ep, NULL);
	assert(res == 0);
	//cout << "bind cq" << endl;
	res = fi_ep_bind(ep, (fid_t) cq, FI_SEND | FI_RECV); //  | FI_TRANSMIT
	assert(res == 0);
	res = fi_enable(ep);
	assert(res == 0);
}

void init_av() {
	cout << "[pid:" << alps_app_pe << "]build address vector" << endl;
	struct fi_av_attr av_attr;
	memset(&av_attr, 0, sizeof(struct fi_av_attr));
	av_attr.type = FI_AV_TABLE;
	av_attr.count = 10;
	int res = fi_av_open(domain, &av_attr, &av, NULL);
	assert(res == 0);
	res = fi_ep_bind(ep, (fid_t) av, 0);
	assert(res == 0);
}

void register_mem() {
	cout << "[pid:" << alps_app_pe << "]register memory" << endl;
	int res = posix_memalign((void**) &buffer, 4096, buffer_size);
	assert(res == 0);
	res = fi_mr_reg(domain, buffer, buffer_size, FI_REMOTE_WRITE, 0, 0, 0, &mr,
			NULL);
	assert(res == 0);
	mr_key = fi_mr_key(mr);

}

void insert_dest_av() {
	struct fi_info *other_info = find_other_addr();
	assert(other_info->dest_addr != NULL);
	int res = fi_av_insert(av, other_info->dest_addr, 1, &fi_addr, 0, NULL);
	//printf("%d\n", fi_addr);
	assert(res == 1);
}

void post_recv() {
	size_t recv_buffer_len = sizeof(recv_buffer);
	int res = fi_recv(ep, &recv_buffer, recv_buffer_len, NULL, fi_addr,
			&fi_recv_context);
	assert(res == 0);
}

void post_send() {
	size_t send_buffer_len = sizeof(send_buffer);
	send_buffer.mr_key = mr_key;
	send_buffer.addr = (uintptr_t) buffer;

	struct fi_msg msg;
	struct iovec iov;
	iov.iov_base = (void*) &send_buffer;
	iov.iov_len = sizeof(send_buffer); //send_buffer_len;

	void *descs;
	descs = NULL;

	msg.msg_iov = &iov;
	msg.desc = &descs;
	msg.iov_count = 1;
	msg.addr = fi_addr;
	msg.context = &fi_send_context;
	msg.data = 14195;

	cout << "[pid:" << alps_app_pe << "] start fi_sendmsg call ...\n";
	ssize_t result = fi_sendmsg(ep, &msg, FI_COMPLETION);
	cout << "[pid:" << alps_app_pe << "] fi_sendmsg call ended with err:"
			<< fi_strerror(-result) << endl;
	assert(result == 0);
}

void writemsg() {
	struct iovec rma_iov;
	rma_iov.iov_base = buffer;
	rma_iov.iov_len = buffer_size;

	struct fi_rma_iov rma_remote_iov;
	rma_remote_iov.addr = remote_addr;
	rma_remote_iov.len = buffer_size;
	rma_remote_iov.key = remote_key;

	void *descs;
	descs = NULL;

	struct fi_msg_rma rma_msg;
	rma_msg.msg_iov = &rma_iov;
	rma_msg.desc = &descs;
	rma_msg.iov_count = 1;
	rma_msg.addr = fi_addr;
	rma_msg.rma_iov = &rma_remote_iov;
	rma_msg.rma_iov_count = 1;
	rma_msg.context = &fi_write1_context;
	rma_msg.data = 14195;
	cout << "[pid:" << alps_app_pe << "] start fi_writemsg call ...\n";
	ssize_t result = fi_writemsg(ep, &rma_msg, FI_COMPLETION);
	cout << "[pid:" << alps_app_pe << "] fi_writemsg call ended! with err:"
			<< result << endl;
	assert(result == 0);
}

void sender_cq_event_handler(struct fi_cq_data_entry event) {
	ssize_t result;

	if ((event.flags & FI_RMA) != 0 || dest_terminated) {
		std::cout << "[pid:" << alps_app_pe << "]FI_RMA call#" << counter << std::endl;
		counter++;
		// terminate
		if (counter == iterations) {
			endT = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> elapsed_seconds = endT - startT;
			cout <<"[pid:" << alps_app_pe << "] bw:"
					<< (buffer_size * 2 * iterations) / elapsed_seconds.count()
							/ 1024.0 / 1024.0 / 1024.0 << " GB/s" << endl;
			exit(1);
		}
		// send_msg to kill the other process
		if (counter == 5/*iterations / 2*/) {
			post_send();
			dest_terminated = true;
			//sleep(5);
		}
		writemsg();
		//if (dest_terminated)
		//	sleep(1);
		return;
	}
	if ((event.flags & FI_RECV) != 0) {
		std::cout << "[pid:" << alps_app_pe << "] FI_RECV is triggered\n";
		//std::cout << "FI_MSG " << event.flags << std::endl;
		//std::cout << "FI_MSG " << event.buf << std::endl;
		startT = std::chrono::high_resolution_clock::now();
		struct mr_message *msg;
		msg = (struct mr_message*) event.buf;
		//std::cout << "FI_MSG " << msg << std::endl;
		remote_key = msg->mr_key;
		remote_addr = msg->addr;
		//std::cout << "FI_MSG alps_app_pe " << alps_app_pe << std::endl;
		writemsg();
		post_recv();
		return;
	}
}

void receiver_cq_event_handler(struct fi_cq_data_entry event) {
	if ((event.flags & FI_RECV) != 0) {
		std::cout << "[pid:" << alps_app_pe << "] FI_RECV is triggered\n";
		//std::cout << "FI_MSG " << event.flags << std::endl;
		//std::cout << "FI_MSG " << event.buf << std::endl;
		struct mr_message *msg;
		msg = (struct mr_message*) event.buf;
		//std::cout << "FI_MSG " << msg << std::endl;
		remote_key = msg->mr_key;
		remote_addr = msg->addr;
		//std::cout << "FI_MSG alps_app_pe " << alps_app_pe << std::endl;

		if (counter == 1) {
			cout << "[pid:" << alps_app_pe << "]force exit after " << counter << "...\n";
			exit(1);
		}
		counter++;
		post_recv();
	}
}
/**
 * MAIN
 */
int main(int argc, char **argv) {
	ip_address = argv[1];
	alps_app_pe = argv[2];

	int res;
	find_psm2();
	init_fabric_domain();
	init_cq();
	init_ep();
	init_av();
	register_mem();

	sleep(10);

	insert_dest_av();

	//printf("fi_recv\n");

	sleep(1);

	post_recv();
	post_send();

	sleep(1);

	//cout << "cq" << endl;
	while (1) {
		struct fi_cq_data_entry event;
		ssize_t read = fi_cq_read(cq, &event, 1);
		if ((read == 1 && 0 == strcmp(alps_app_pe, "1")) || dest_terminated) {
			sender_cq_event_handler(event);
		}
		if (read == 1 && 0 == strcmp(alps_app_pe, "0")) {
			receiver_cq_event_handler(event);
		}
	}

	return 0;
}
