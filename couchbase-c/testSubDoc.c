#include <libcouchbase/couchbase.h>
#include <libcouchbase/api3.h>
#include <libcouchbase/pktfwd.h>
#include <libcouchbase/views.h>
#include <libcouchbase/ixmgmt.h>
#include <libcouchbase/n1ql.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <string>
#include <cinttypes>

struct Result {
    lcb_error_t rc;
    std::string key;
    std::string value;
    lcb_CAS cas;

    explicit Result(const lcb_RESPBASE *rb)
        : rc(rb->rc), key(reinterpret_cast< const char * >(rb->key), rb->nkey), cas(rb->cas)
    {
    }
};

struct Rows {
    std::vector< std::string > rows;
    std::string metadata;
    lcb_error_t rc;
    short htcode;
    Rows() : rc(LCB_ERROR), htcode(0) {}
};

typedef std::vector< Result > ResultList;

void subdoc_callback(lcb_t instance, int cbtype, const lcb_RESPBASE *rb)
{
    const lcb_RESPSUBDOC *resp = (const lcb_RESPSUBDOC *)rb;

    printf("SUBDOC CALLBACK\n");
    if (rb->rc == LCB_SUCCESS) {
        printf("%.*s. CAS=0x%" PRIx64 "\n", (int)rb->nkey, (char *)rb->key, rb->cas);
    } else {
        printf("%.*s. ERROR: %s\n", (int)rb->nkey, (char *)rb->key, lcb_strerror_short(rb->rc));
    }
    lcb_SDENTRY cur;
    size_t vii = 0, oix = 0;
    while (lcb_sdresult_next(resp, &cur, &vii)) {
        int index = oix++;
        /* mutations might skip explit entry if everything is good,
         *            but when they do have entry, 'index' must be used */
        if (cbtype == LCB_CALLBACK_SDMUTATE) {
            index = cur.index;
        }
        printf("%d. Size=%lu, RC=%s\n", index, (unsigned long)cur.nvalue,
                lcb_strerror_short(cur.status));
        if (cur.nvalue > 0) {
            fwrite(cur.value, 1, cur.nvalue, stdout);
            printf("\n");
        }
    }
}


static void query_callback(lcb_t, int, const lcb_RESPN1QL *resp)
{
    Rows *rows = reinterpret_cast< Rows * >(resp->cookie);

    // Check if this is the last invocation
    if (resp->rflags & LCB_RESP_F_FINAL) {
        rows->rc = resp->rc;

        // Assign the metadata (usually not needed)
        rows->metadata.assign(resp->row, resp->nrow);

    } else {
        rows->rows.push_back(std::string(resp->row, resp->nrow));
    }
}

static void die(lcb_error_t rc, const char *msg)
{
    fprintf(stderr, "%s failed. (0x%x, %s)\n", msg, rc, lcb_strerror(NULL, rc));
    exit(EXIT_FAILURE);
}

int main()
{
    lcb_t instance;
    struct lcb_create_st cropts = {0};
    lcb_error_t rc;

    cropts.version = 3;
    //cropts.v.v3.connstr = "couchbase://172.30.0.110/testbucket";
    cropts.v.v3.connstr = "couchbase://172.16.111.182/test";
    cropts.v.v3.username = "admin";
    cropts.v.v3.passwd = "5g2o2o";

    rc = lcb_create(&instance, &cropts);
    rc = lcb_cntl_string(instance,"detailed_errcodes","true");

    if (rc != LCB_SUCCESS) {
        die(rc, "Creating instance");
    }

    rc = lcb_connect(instance);
    if (rc != LCB_SUCCESS) {
        die(rc, "Connection scheduling");
    }

    /* This function required to actually schedule the operations on the network */
    lcb_wait(instance);

    /* Determines if the bootstrap/connection succeeded */
    rc = lcb_get_bootstrap_status(instance);
    if (rc != LCB_SUCCESS) {
        die(rc, "Connection bootstraping");
    } else {
        printf("Connection succeeded. Cluster has %d nodes\n", lcb_get_num_nodes(instance));
    }

    lcb_install_callback3(instance, LCB_CALLBACK_SDLOOKUP, (lcb_RESPCALLBACK)subdoc_callback);
    lcb_install_callback3(instance, LCB_CALLBACK_SDMUTATE, (lcb_RESPCALLBACK)subdoc_callback);

    //INSERTING RECORDS
    std::map< std::string, std::string > toStore;
    toStore["foo"] = "{\"value\":\"fooValue\"}";
    toStore["bar"] = "{\"value\":\"barValue\"}";
    toStore["baz"] = "{\"value\":\"bazValue\"}";

    ResultList results;


    lcb_sched_enter(instance);
    std::map< std::string, std::string >::const_iterator its = toStore.begin();
    for (; its != toStore.end(); ++its) {
        lcb_CMDSTORE scmd = {};
        LCB_CMD_SET_KEY(&scmd, its->first.c_str(), its->first.size());
        LCB_CMD_SET_VALUE(&scmd, its->second.c_str(), its->second.size());
        scmd.operation = LCB_SET;
        rc = lcb_store3(instance, &results, &scmd);
        if (rc != LCB_SUCCESS) {
            fprintf(stderr, "Couldn't schedule item %s: %s\n", its->first.c_str(), lcb_strerror(NULL, rc));

            lcb_sched_fail(instance);
            break;
        }
    }

    lcb_sched_leave(instance);
    lcb_wait(instance);

    //SUBDOCUMENT GET
    lcb_SDSPEC spec = {0};
    spec.sdcmd = LCB_SDCMD_GET;
    const char *path = "guami.plmnId.mcc";
    LCB_SDSPEC_SET_PATH(&spec, path, strlen(path));

    lcb_CMDSUBDOC cmd = {0};
    const char *key = "amit5";
    LCB_CMD_SET_KEY(&cmd, key, strlen(key));
    cmd.nspecs = 1;
    cmd.specs = &spec;
    lcb_error_t err = lcb_subdoc3(instance, NULL, &cmd);

    if (err != LCB_SUCCESS) {
        printf("Unable to schedule subdoc: %s\n", lcb_strerror_short(err));
    }
    lcb_wait(instance);

    //SUBDOCUMENT update
    /*lcb_SDSPEC spec = {0};
      spec.sdcmd = LCB_SDCMD_DICT_UPSERT;
      const char *path = "guami.plmnId.mcc";
      LCB_SDSPEC_SET_PATH(&spec, path, strlen(path));
      const char *value = "\"updatedValue\"";
      LCB_SDSPEC_SET_VALUE(&spec, value, strlen(value));

      lcb_CMDSUBDOC cmd = {0};
      const char *key = "amit5";
      LCB_CMD_SET_KEY(&cmd, key, strlen(key));
      cmd.nspecs = 1;
      cmd.specs = &spec;
      lcb_error_t err = lcb_subdoc3(instance, NULL, &cmd);
      if (err != LCB_SUCCESS) {
      printf("Unable to schedule subdoc: %s\n", lcb_strerror_short(err));
      }
      lcb_wait(instance);*/

    lcb_destroy(instance);
    return 0;
}
