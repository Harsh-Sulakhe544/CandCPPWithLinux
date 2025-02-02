// REFER : client.cpp 
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <assert.h> // for assert()
#include <math.h> // for mathematical operations 
#include <vector> // for mapping clients with its fd 
#include <fcntl.h> // for fcntl()
#include <poll.h> // for pollin and pollout 
#include <time.h> // for signal-handling with timers wrt poll 
#include <string> // for std::string 
// #include <map> // std::map

// proj
#include "hashtable.h"
#include "zset.h"
#include "common.h"
#include "list.h"
#include "heap.h"
#include "thread_pool.h"

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
    int err = errno;
    // difference "https://www.geeksforgeeks.org/difference-printf-sprintf-fprintf/" or (docs : man frintf)
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

// get_monotonic_usec is the function for getting the time. Note that the timestamp must be monotonic. Timestamp jumping backward can cause all sorts of troubles in computer systems.
static uint64_t get_monotonic_usec() {
    timespec tv = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return uint64_t(tv.tv_sec) * 1000000 + tv.tv_nsec / 1000;
}

// used event-loops 
static void fd_set_nb(int fd ) {
	errno = 0; 
	int flags = fcntl(fd, F_GETFL, 0); 
	
	if(errno) {
		die("fcntl error from server ");
        return;
	}
	
	errno = 0;
	flags |= O_NONBLOCK; // make it non-block mode for read() 
	(void)fcntl(fd, F_SETFL, flags); // f-set-flag : F_SETFL
	
	if(errno) {
		die("fcntl error");
	}
}

// 1 KB	= 1024 Bytes
const size_t k_max_msg  = 4096; // 4 kb 

/* char rbuf[4 + k_max_msg + 1];
 4 bytes for some additional data (possibly headers or control information),
    4096 bytes for the maximum message size,
    1 byte for the null terminator, which is essential for C-style strings to indicate the end of the string.
    
total     4101 bytes,
*/

enum {
	STATE_REQ,  // REQUEST - 0 
	STATE_RES, // RESPONSE - 1
	STATE_END, // MARK THE CONNECTION FOR DELETION  - 2
}; 

struct Conn {
	int fd = -1; 
	uint32_t state = 0;  // either of STATE_REQ, or STATE_RESP
	 
	// We need buffers for reading/writing, since in nonblocking mode, IO operations are often deferred. 
	
	// buffer for reading 
	size_t rbuf_size = 0; // to handle the number of at-a-time requests from clients
    uint8_t rbuf[4 + k_max_msg];  
    
    // buffer for writing
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];	
    
    uint64_t idle_start = 0;
    // timer
    DList idle_list;
	
};

// put the connection in vector 
static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn) {
	/* resize if less */ 
	if (fd2conn.size() <= (size_t)conn->fd) {
        fd2conn.resize(conn->fd + 1);
    }
    
    fd2conn[conn->fd] = conn;	// add that connection to fd 
}

// The data structure for the key space.
static struct {
    HMap db;
    // a map of all client connections, keyed by fd
    std::vector<Conn *> fd2conn;
    // timers for idle connections
    DList idle_list;
    // timers for TTLs
    std::vector<HeapItem> heap;
    
    // the thread pool
    ThreadPool tp;
    
} g_data;

static int32_t accept_new_conn(std::vector<Conn *> &fd2conn, int fd) {
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr); // length of the socket 
    
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen); // accept the connection fd 
    if (connfd < 0) {
        msg("accept() error from server ");
        return -1;  // error
    }

    // set the new connection fd to nonblocking mode
    fd_set_nb(connfd);
    
    // creating the struct Conn
    struct Conn *conn = (struct Conn *)malloc(sizeof(struct Conn));
    
    if (!conn) {
        close(connfd);
        return -1;
    }
    
    // assign the attributes of connection
    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    conn_put(fd2conn, conn);
    
    conn->idle_start = get_monotonic_usec();
    dlist_insert_before(&g_data.idle_list, &conn->idle_list);
    conn_put(g_data.fd2conn, conn);
    
    return 0;
}

static void state_req(Conn *conn);
static void state_res(Conn *conn);


const size_t k_max_args = 1024;
// parsing request 

static int32_t parse_req( const uint8_t *data, size_t len, std::vector<std::string> &out) {
    // we at-least neeed 4 bytes of data for parsing request 
    if (len < 4) {
        return -1;
    }
    
    uint32_t n = 0;
    memcpy(&n, &data[0], 4);
    
    if (n > k_max_args) {
        return -1;
    }

    size_t pos = 4; // start from position 4 , 
    while (n--) {
    // if we get only command like get 
        if (pos + 4 > len) {
            return -1;
        }
        
        uint32_t sz = 0; // size of the current-string being parsed (how many bytes for to extract for current string )
        memcpy(&sz, &data[pos], 4);
        
        // chek can we still read , do it in next iteration
        if (pos + 4 + sz > len) {
            return -1;
        }
        
        out.push_back(std::string((char *)&data[pos + 4], sz));
        pos += 4 + sz; // to skip the size for next-iteration 
    }
	
	// check extra bytes are getting fetched , if so remove 
    if (pos != len) {
        return -1;  // trailing garbage
    }
    
    return 0;
}


enum {
    T_STR = 0,  // string
    T_ZSET = 1, // sorted set
};

// the structure for the key
struct Entry {
     struct HNode node;
    std::string key;
    std::string val;
    uint32_t type = 0;
    ZSet *zset = NULL;
    // for TTLs
    size_t heap_idx = -1;
};

// to check equality of 2 nodes 
static bool entry_eq(HNode *lhs, HNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

enum {
    ERR_UNKNOWN = 1, // unknown-error
    ERR_2BIG = 2, // too-big error 
    ERR_TYPE = 3,
    ERR_ARG = 4,
};

// serializaion-protocol : 
static void out_nil(std::string& out) {
	out.push_back(SER_NIL);
}

// push the output-string serial-token, bytes, val(orginal string value) into out vector
static void out_str(std::string &out, const char *s, size_t size) {
    out.push_back(SER_STR);
    uint32_t len = (uint32_t)size;
    out.append((char *)&len, 4);
    out.append(s, len);
}

static void out_str(std::string &out, const std::string &val) {
    return out_str(out, val.data(), val.size());
}

// to push an output of 8 bytes int64_t into out vector  
static void out_int(std::string &out, int64_t val) {
    out.push_back(SER_INT);
    out.append((char *)&val, 8);
}

static void out_dbl(std::string &out, double val) {
    out.push_back(SER_DBL);
    out.append((char *)&val, 8);
}

// error pushing in to out vector 
static void out_err(std::string &out, int32_t code, const std::string &msg) {
    out.push_back(SER_ERR);
    out.append((char *)&code, 4);
    uint32_t len = (uint32_t)msg.size();
    out.append((char *)&len, 4);
    out.append(msg);
}

// to push an array in to out vector
static void out_arr(std::string &out, uint32_t n) {
    out.push_back(SER_ARR);
    out.append((char *)&n, 4);
}

static void *begin_arr(std::string &out) {
    out.push_back(SER_ARR);
    out.append("\0\0\0\0", 4);          // filled in end_arr()
    return (void *)(out.size() - 4);    // the `ctx` arg
}

static void end_arr(std::string &out, void *ctx, uint32_t n) {
    size_t pos = (size_t)ctx;
    assert(out[pos - 1] == SER_ARR);
    memcpy(&out[pos], &n, 4);
}
 
static void do_get(std::vector<std::string> &cmd, std::string &out) {
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!node) {
        return out_nil(out);
    }

    Entry *ent = container_of(node, Entry, node);
    if (ent->type != T_STR) {
        return out_err(out, ERR_TYPE, "expect string type");
    }
    
    return out_str(out, ent->val);
}

static void do_set(std::vector<std::string> &cmd, std::string &out) {
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (node) {
        Entry *ent = container_of(node, Entry, node);
        if (ent->type != T_STR) {
            return out_err(out, ERR_TYPE, "expect string type");
        }
        ent->val.swap(cmd[2]);
    } else {
        Entry *ent = new Entry();
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        ent->val.swap(cmd[2]);
        hm_insert(&g_data.db, &ent->node);
    }
    return out_nil(out);
}

// set or remove the TTL
static void entry_set_ttl(Entry *ent, int64_t ttl_ms) {
    if (ttl_ms < 0 && ent->heap_idx != (size_t)-1) {
        // erase an item from the heap
        // by replacing it with the last item in the array.
        size_t pos = ent->heap_idx;
        g_data.heap[pos] = g_data.heap.back();
        g_data.heap.pop_back();
        if (pos < g_data.heap.size()) {
            heap_update(g_data.heap.data(), pos, g_data.heap.size());
        }
        ent->heap_idx = -1;
    } else if (ttl_ms >= 0) {
        size_t pos = ent->heap_idx;
        if (pos == (size_t)-1) {
            // add an new item to the heap
            HeapItem item;
            item.ref = &ent->heap_idx;
            g_data.heap.push_back(item);
            pos = g_data.heap.size() - 1;
        }
        g_data.heap[pos].val = get_monotonic_usec() + (uint64_t)ttl_ms * 1000;
        heap_update(g_data.heap.data(), pos, g_data.heap.size());
    }
}

static bool str2int(const std::string &s, int64_t &out) {
    char *endp = NULL;
    out = strtoll(s.c_str(), &endp, 10);
    return endp == s.c_str() + s.size();
}

// new commands : 
static void do_expire(std::vector<std::string> &cmd, std::string &out) {
    int64_t ttl_ms = 0;
    if (!str2int(cmd[2], ttl_ms)) {
        return out_err(out, ERR_ARG, "expect int64");
    }

    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (node) {
        Entry *ent = container_of(node, Entry, node);
        entry_set_ttl(ent, ttl_ms);
    }
    return out_int(out, node ? 1: 0);
}

static void do_ttl(std::vector<std::string> &cmd, std::string &out) {
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!node) {
        return out_int(out, -2);
    }

    Entry *ent = container_of(node, Entry, node);
    if (ent->heap_idx == (size_t)-1) {
        return out_int(out, -1);
    }

    uint64_t expire_at = g_data.heap[ent->heap_idx].val;
    uint64_t now_us = get_monotonic_usec();
    return out_int(out, expire_at > now_us ? (expire_at - now_us) / 1000 : 0);
}

// CHECK THE ENTITY TYPE and  
// deallocate the key immediately
static void entry_destroy(Entry *ent) {
    switch (ent->type) {
    case T_ZSET:
        zset_dispose(ent->zset);
        delete ent->zset;
        break;
    }
    delete ent;
}

static void entry_del_async(void *arg) {
    entry_destroy((Entry *)arg);
}

// dispose the entry after it got detached from the key space
static void entry_del(Entry *ent) {
    entry_set_ttl(ent, -1);

    const size_t k_large_container_size = 10000;
    bool too_big = false;
    switch (ent->type) {
    case T_ZSET:
        too_big = hm_size(&ent->zset->hmap) > k_large_container_size;
        break;
    }

    if (too_big) {
        thread_pool_queue(&g_data.tp, &entry_del_async, ent);
    } else {
        entry_destroy(ent);
    }
}


static void do_del(std::vector<std::string> &cmd, std::string &out) {
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_pop(&g_data.db, &key.node, &entry_eq);
    if (node) {
         entry_del(container_of(node, Entry, node));
    }
    return out_int(out, node ? 1 : 0);
}

// scan the hash-table 
static void h_scan(HTab *tab, void (*f)(HNode *, void *), void *arg) {
// if there is nothing in the table s
    if (tab->size == 0) {
        return;
    }
    
    for (size_t i = 0; i < tab->mask + 1; ++i) {
        HNode *node = tab->tab[i];
        while (node) {
            f(node, arg);
            node = node->next;
        }
    }
}

// this is a call-back function ==> void (*f)(HNode *, void *) from h_scan() ==> this is a call-back-scan() 
static void cb_scan(HNode *node, void *arg) {
    std::string &out = *(std::string *)arg;
    out_str(out, container_of(node, Entry, node)->key);
}

static void do_keys(std::vector<std::string> &cmd, std::string &out) {
    (void)cmd;
    out_arr(out, (uint32_t)hm_size(&g_data.db));
    // check is there any-data in the hash-table to print 
    h_scan(&g_data.db.ht1, &cb_scan, &out); // scan hash-table-1
    h_scan(&g_data.db.ht2, &cb_scan, &out); // scan hash-table-2
}

// string to double 
static bool str2dbl(const std::string &s, double &out) {
    char *endp = NULL;
    out = strtod(s.c_str(), &endp);
    return endp == s.c_str() + s.size() && !isnan(out);
}


// zadd zset score name
static void do_zadd(std::vector<std::string> &cmd, std::string &out) {
    double score = 0;
    if (!str2dbl(cmd[2], score)) {
        return out_err(out, ERR_ARG, "expect fp number");
    }

    // look up or create the zset
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *hnode = hm_lookup(&g_data.db, &key.node, &entry_eq);

    Entry *ent = NULL;
    if (!hnode) {
        ent = new Entry();
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        ent->type = T_ZSET;
        ent->zset = new ZSet();
        hm_insert(&g_data.db, &ent->node);
    } else {
        ent = container_of(hnode, Entry, node);
        if (ent->type != T_ZSET) {
            return out_err(out, ERR_TYPE, "expect zset");
        }
    }

    // add or update the tuple
    const std::string &name = cmd[3];
    bool added = zset_add(ent->zset, name.data(), name.size(), score);
    return out_int(out, (int64_t)added);
}

static bool expect_zset(std::string &out, std::string &s, Entry **ent) {
    Entry key;
    key.key.swap(s);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *hnode = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!hnode) {
        out_nil(out);
        return false;
    }

    *ent = container_of(hnode, Entry, node);
    if ((*ent)->type != T_ZSET) {
        out_err(out, ERR_TYPE, "expect zset");
        return false;
    }
    return true;
}

// zrem zset name
static void do_zrem(std::vector<std::string> &cmd, std::string &out) {
    Entry *ent = NULL;
    if (!expect_zset(out, cmd[1], &ent)) {
        return;
    }

    const std::string &name = cmd[2];
    ZNode *znode = zset_pop(ent->zset, name.data(), name.size());
    if (znode) {
        znode_del(znode);
    }
    return out_int(out, znode ? 1 : 0);
}

// zscore zset name
static void do_zscore(std::vector<std::string> &cmd, std::string &out) {
    Entry *ent = NULL;
    if (!expect_zset(out, cmd[1], &ent)) {
        return;
    }

    const std::string &name = cmd[2];
    ZNode *znode = zset_lookup(ent->zset, name.data(), name.size());
    return znode ? out_dbl(out, znode->score) : out_nil(out);
}

// zquery zset score name offset limit
static void do_zquery(std::vector<std::string> &cmd, std::string &out) {
    // parse args
    double score = 0;
    if (!str2dbl(cmd[2], score)) {
        return out_err(out, ERR_ARG, "expect fp number");
    }
    const std::string &name = cmd[3];
    int64_t offset = 0;
    int64_t limit = 0;
    if (!str2int(cmd[4], offset)) {
        return out_err(out, ERR_ARG, "expect int");
    }
    if (!str2int(cmd[5], limit)) {
        return out_err(out, ERR_ARG, "expect int");
    }

    // get the zset
    Entry *ent = NULL;
    if (!expect_zset(out, cmd[1], &ent)) {
        if (out[0] == SER_NIL) {
            out.clear();
            out_arr(out, 0);
        }
        return;
    }

    // look up the tuple
    if (limit <= 0) {
        return out_arr(out, 0);
    }
    ZNode *znode = zset_query(ent->zset, score, name.data(), name.size());
    znode = znode_offset(znode, offset);

    // output
    void *arr = begin_arr(out);
    uint32_t n = 0;
    while (znode && (int64_t)n < limit) {
        out_str(out, znode->name, znode->len);
        out_dbl(out, znode->score);
        znode = znode_offset(znode, +1);
        n += 2;
    }
    end_arr(out, arr, n);
}


// check the command 
static bool cmd_is(const std::string &word, const char *cmd) {
    // for c_str() ==> "https://www.geeksforgeeks.org/basic_string-c_str-function-in-c-stl/"
    return 0 == strcasecmp(word.c_str(), cmd);
}

// The do_request function handles the request. Only 3 commands (get, set, del) are recognized now.
static void do_request(std::vector<std::string> &cmd, std::string &out) {
    if (cmd.size() == 1 && cmd_is(cmd[0], "keys")) {
        do_keys(cmd, out);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {
        do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
        do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {
        do_del(cmd, out);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "pexpire")) {
        do_expire(cmd, out);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "pttl")) {
        do_ttl(cmd, out);
    } else if (cmd.size() == 4 && cmd_is(cmd[0], "zadd")) {
        do_zadd(cmd, out);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "zrem")) {
        do_zrem(cmd, out);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "zscore")) {
        do_zscore(cmd, out);
    } else if (cmd.size() == 6 && cmd_is(cmd[0], "zquery")) {
        do_zquery(cmd, out);
    } else {
        // cmd is not recognized
        out_err(out, ERR_UNKNOWN, "Unknown cmd");
    }
}


// try to process one request at a time 
static bool try_one_request(Conn *conn) {
	// check size 
	if(conn->rbuf_size < 4) {
		return false; // not enough data in the buffer, will retur in the next iteration
	}
	
	uint32_t len = 0; 
	memcpy(&len, &conn->rbuf[0], 4); // copy the header in to the len 
	if (len > k_max_msg) {
        msg("too long message than header ");
        conn->state = STATE_END;
        return false;
    }
    
    // check is ther any data 
    if (4 + len > conn->rbuf_size) {
        // not enough data in the buffer. Will retry in the next iteration
        return false;
    }
    
    // parse the request
    std::vector<std::string> cmd;
    if (0 != parse_req(&conn->rbuf[4], len, cmd)) {
        msg("bad req");
        conn->state = STATE_END;
        return false;
    }

    // got one request, generate the response.
    std::string out;
    do_request(cmd, out);
    
	// pack the response into the buffer
    if (4 + out.size() > k_max_msg) {
        out.clear();
        out_err(out, ERR_2BIG, "response is too big");
    }
    
    uint32_t wlen = (uint32_t)out.size();
    
    // generating echoing response
    memcpy(&conn->wbuf[0], &wlen, 4);
    memcpy(&conn->wbuf[4], out.data(), out.size());
    conn->wbuf_size = 4 + wlen;
    
      // remove the request from the buffer.
    // note: frequent memmove is inefficient.
    // note: need better handling for production code.
    
    size_t remain = conn->rbuf_size - 4 - len; 
    if(remain) {
    // void *memmove(void *dest, const void *src, size_t n);
    	memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
    }
    
    conn->rbuf_size = remain;
    
    // change state
    conn->state = STATE_RES;
    state_res(conn);
    
     // continue the outer loop if the request was fully processed
    return (conn->state == STATE_REQ);
}

// try to fill the buffer for the request 
static bool try_fill_buffer(Conn *conn) {
	
	// check for size of read-buffer , is it less than a single connection 
	assert(conn->rbuf_size < sizeof(conn->rbuf));
	 ssize_t rv = 0;
	 do{
	 // capture it
	 	size_t cap = sizeof(conn->rbuf) - conn->rbuf_size; 
	 	rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap); // till we recieve all bytes 
	 }while (rv < 0 && errno == EINTR);
	 
	 if(rv < 0 && errno == EAGAIN) {
	 	// got EAGAIN, stop.
        return false;
	 }
	 
	 // if all bits are still not recieved : 
	 if (rv < 0) {
        msg("read() error, cannot recieved all bits");
        conn->state = STATE_END;
        return false;
    }
    
    // if all bits are succesfully recieved 
    if(rv == 0) {
    	// CHECK BUFFER OVERFLOW 
    	if(conn->rbuf_size > 0) {
    		msg("UNEXPECTED EOF ");
    	}
    	// SUCCESSFULL 
    	else {
            msg("EOF");
        }
        conn->state = STATE_END; 
        return false; 
    }
    // add the data 
    conn->rbuf_size += (size_t)rv; // filled with rv bytes
    assert(conn->rbuf_size <= sizeof(conn->rbuf));
    
     // Try to process requests one by one.
      // Why is there a loop? Please read the explanation of "pipelining". in details.txt
     while(try_one_request(conn)) {}
     
     return (conn->state == STATE_REQ);
}

// request 
static void state_req(Conn *conn) {
    while (try_fill_buffer(conn)) {}
}


// change from request->response->request 
static bool try_flush_buffer(Conn *conn) {
    ssize_t rv = 0;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while (rv < 0 && errno == EINTR);
    
    if (rv < 0 && errno == EAGAIN) {
        // got EAGAIN, stop.
        return false;
    }
    if (rv < 0) {
        msg("write() error from server ");
        conn->state = STATE_END;
        return false;
    }
    
    conn->wbuf_sent += (size_t)rv; // data added to the write buffer 
    
    assert(conn->wbuf_sent <= conn->wbuf_size);
    
    if (conn->wbuf_sent == conn->wbuf_size) {
        // response was fully sent, change state back
        conn->state = STATE_REQ;
        conn->wbuf_sent = 0;
        conn->wbuf_size = 0;
        return false;
    }
    // still got some data in wbuf, could try to write again
    return true;
}

// response 
static void state_res(Conn *conn) {
    while (try_flush_buffer(conn)) {}
}

static void connection_io(Conn *conn) {
	// waked up by poll, update the idle timer
    // by moving conn to the end of the list.
    conn->idle_start = get_monotonic_usec();
    dlist_detach(&conn->idle_list);
    dlist_insert_before(&g_data.idle_list, &conn->idle_list);
    
    if (conn->state == STATE_REQ) {
        state_req(conn);
    } else if (conn->state == STATE_RES) {
        state_res(conn);
    } else {
        assert(0);  // not expected
    }
}

// The next_timer_ms function takes the first (nearest) timer from the list and uses it the calculate the timeout value of poll.
const uint64_t k_idle_timeout_ms = 5 * 1000;

static uint32_t next_timer_ms() {
    uint64_t now_us = get_monotonic_usec();
    uint64_t next_us = (uint64_t)-1;

    // idle timers
    if (!dlist_empty(&g_data.idle_list)) {
        Conn *next = container_of(g_data.idle_list.next, Conn, idle_list);
        next_us = next->idle_start + k_idle_timeout_ms * 1000;
    }

    // ttl timers
    if (!g_data.heap.empty() && g_data.heap[0].val < next_us) {
        next_us = g_data.heap[0].val;
    }

    if (next_us == (uint64_t)-1) {
        return 10000;   // no timer, the value doesn't matter
    }

    if (next_us <= now_us) {
        // missed?
        return 0;
    }
    return (uint32_t)((next_us - now_us) / 1000);
}

// Don’t forget to remove the connection from the list when done:
static void conn_done(Conn *conn) {
    g_data.fd2conn[conn->fd] = NULL;
    (void)close(conn->fd);
    dlist_detach(&conn->idle_list);
    free(conn);
}

// check the lhs and rhs of a node 
static bool hnode_same(HNode *lhs, HNode *rhs) {
    return lhs == rhs;
}

// Fire Timers : At each iteration of the event loop, the list is checked in order to fire timers in due time.
static void process_timers() {
    // the extra 1000us is for the ms resolution of poll()
    uint64_t now_us = get_monotonic_usec() + 1000;

    // idle timers
    while (!dlist_empty(&g_data.idle_list)) {
        Conn *next = container_of(g_data.idle_list.next, Conn, idle_list);
        uint64_t next_us = next->idle_start + k_idle_timeout_ms * 1000;
        if (next_us >= now_us) {
            // not ready
            break;
        }

        printf("removing idle connection: %d\n", next->fd);
        conn_done(next);
    }

    // TTL timers
    const size_t k_max_works = 2000;
    size_t nworks = 0;
    while (!g_data.heap.empty() && g_data.heap[0].val < now_us) {
        Entry *ent = container_of(g_data.heap[0].ref, Entry, heap_idx);
        HNode *node = hm_pop(&g_data.db, &ent->node, &hnode_same);
        assert(node == &ent->node);
        entry_del(ent);
        if (nworks++ >= k_max_works) {
            // don't stall the server if too many keys are expiring at once
            break;
        }
    }
}
int main() {
	
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    // this is needed for most server applications
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);    // wildcard address 0.0.0.0
    
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr)); // RECIEVE 
    if (rv) {
        die("bind()");
    }

    // listen
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen() from server ");
    }
    
    /* man poll : from docs : 
    refer : "struct pollfd {
    int   fd;         // file descriptor 
    short events;     // requested events //
    short revents;    // returned events //
    }" */
    
    // a map of all client connections, keyed by fd
    std::vector<Conn *>fd2conn; // point to the struct , so we used * 
    
    // set the listen fd to nonblocking mode
    fd_set_nb(fd);
	
	// some initializations
    dlist_init(&g_data.idle_list);
    thread_pool_init(&g_data.tp, 4); // create 4 threads 
        
    // create a event loop , with a empty structure 
    std::vector<struct pollfd> poll_args; 
    
    while(true) {
    	// prepare the arguements of the poll()
    	poll_args.clear();
    	
    	// prepare the pollfd 
    	struct pollfd pfd = {fd, POLLIN, 0}; // fd-is key , so put in 1st location  , POLLIN - poll input 
    	poll_args.push_back(pfd);
    	// check connections fd's 
    	for(Conn *conn : fd2conn) {
    		// if there is no fd 
    		if(!conn) {
    			continue;
    		}
    		
    		// for 1st time 
    		struct pollfd pfd = {}; 
    		pfd.fd = conn->fd; 
    		pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;  // check is it a request or response
    		pfd.events = pfd.events | POLLERR ; // CHECK IS IT A ERROR 
    		poll_args.push_back(pfd); 
    		
    	}
    	// poll for active fds
        // the timeout argument doesn't matter here
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        if (rv < 0) {
            die("poll");
        }
        
        // process active connections
        for (size_t i = 1; i < poll_args.size(); ++i) {
            // check is it a response 
            if (poll_args[i].revents) {
                Conn *conn = fd2conn[poll_args[i].fd]; // get fd 
                connection_io(conn);
                if (conn->state == STATE_END) {
                    // client closed normally, or something bad happened.
                    // destroy this connection

/*                    fd2conn[conn->fd] = NULL;
                    (void)close(conn->fd);
                    free(conn); */
                    
                    conn_done(conn);
                }
            }
        }
        
        // handle timers
        process_timers();
        
        // try to accept a new connection if the listening fd is active
        if (poll_args[0].revents) {
            (void)accept_new_conn(fd2conn, fd);
        }
        
    }
		
    return 0;
}
