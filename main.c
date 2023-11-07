#include <stdatomic.h>
#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<unistd.h>

#include "/home/gabrielvieira/oran/flexric/src/xApp/e42_xapp_api.h"
#include "/home/gabrielvieira/oran/flexric/src/util/alg_ds/alg/defer.h"
#include "/home/gabrielvieira/oran/flexric/src/util/time_now_us.h"
#include "/home/gabrielvieira/oran/flexric/src/sm/kpm_sm_v2.02/kpm_sm_id.h"
#include "/home/gabrielvieira/oran/flexric/src/sm/slice_sm/slice_sm_id.h"


_Atomic
uint16_t assoc_rnti = 0;

static
void sm_cb_slice(sm_ag_if_rd_t const* rd)
{
  printf("***Rd type:%d, Slice status:%d*** \n\n", rd->type, SLICE_STATS_V0);
  sleep(10);
  assert(rd != NULL);
  assert(rd->type == SLICE_STATS_V0);

  int64_t now = time_now_us();

  printf("SLICE ind_msg latency = %ld \n", now - rd->slice_stats.msg.tstamp);
  if (rd->slice_stats.msg.ue_slice_conf.len_ue_slice > 0)
    assoc_rnti = rd->slice_stats.msg.ue_slice_conf.ues->rnti; // TODO: assign the rnti after get the indication msg
}

static void sm_callback(sm_ag_if_rd_t const* rd){
	printf("***Callback executed***\n");
	printf("KPM Type: %d \n", KPM_STATS_V0);

	assert(rd != NULL);
  	assert(rd->type == KPM_STATS_V0);

    	const kpm_ind_msg_t* msg = &rd->kpm_stats.msg;
      	for(size_t i = 0; i < msg->MeasData_len; i++){
	MeasInfo_t* curMeasInfo = &msg->MeasInfo[i];
	adapter_MeasDataItem_t* curMeasData = &msg->MeasData[i];
		for(size_t j = 0; j < curMeasData->measRecord_len; j++){
			adapter_MeasRecord_t* curMeasRecord = &(curMeasData->measRecord[j]);
			printf("Received RIC Indication:\n");
			printf("---Metric: %s: Value: %li\n", curMeasInfo->measName.buf, curMeasRecord->int_val);
        }
    }
	printf("***Callback finished***\n\n");

}


static
void fill_del_slice(del_slice_conf_t* del)
{
  assert(del != NULL);

  /// SET DL ID ///
  uint32_t dl_ids[] = {2};
  del->len_dl = sizeof(dl_ids)/sizeof(dl_ids[0]);
  if (del->len_dl > 0)
    del->dl = calloc(del->len_dl, sizeof(uint32_t));
  for (uint32_t i = 0; i < del->len_dl; i++) {
    del->dl[i] = dl_ids[i];
    printf("DEL DL SLICE: id %u\n", dl_ids[i]);
  }

  /*
  /// SET UL ID ///
  uint32_t ul_ids[] = {0};
  del->len_ul = sizeof(ul_ids)/sizeof(ul_ids[0]);
  if (del->len_ul > 0)
    del->ul = calloc(del->len_ul, sizeof(uint32_t));
  for (uint32_t i = 0; i < del->len_ul; i++)
    del->ul[i] = ul_ids[i];
  */

}

static
sm_ag_if_wr_t fill_slice_sm_ctrl_req(uint16_t ran_func_id, slice_ctrl_msg_e type)
{
  assert(ran_func_id == 145);

  sm_ag_if_wr_t wr = {0};
  wr.type = SM_AGENT_IF_WRITE_V0_END;
  if (ran_func_id == 145) {
    wr.type = SLICE_CTRL_REQ_V0;
    wr.slice_req_ctrl.hdr.dummy = 0;

    if (type == SLICE_CTRL_SM_V0_DEL) {
      /// DEL ///
      wr.slice_req_ctrl.msg.type = SLICE_CTRL_SM_V0_DEL;
      fill_del_slice(&wr.slice_req_ctrl.msg.u.del_slice);
    } else {
      assert(0 != 0 && "Unknown slice ctrl type");
    }
  } else {
    assert(0 !=0 && "Unknown RAN function id");
  }
  return wr;
}

int main(int argc, char* argv[]){
	// Any code here will be ignored since we init args after following functions

	fr_args_t args = init_fr_args(argc,argv);
	init_xapp_api(&args);

	// Find e2 nodes
	e2_node_arr_t nodes = e2_nodes_xapp_api();
	printf("*****Init xAPP*****\n");
	if(nodes.len > 0){
		printf("**Explain E2 Node**\n");
		printf(
			"RAN function ID: %d \n", 
			nodes.n[0].ack_rf[0].id
		);
		printf("**End Explain**\n");
	}
	
	inter_xapp_e inter = ms_5;

	// returns a handle
	sm_ans_xapp_t report_handle = report_sm_xapp_api(&nodes.n[0].id, nodes.n[0].ack_rf[0].id, inter, &sm_callback);
	
	// Remove the handle previously returned
	rm_report_sm_xapp_api(report_handle.u.handle);

	printf("***Creating another handler for the control message***\n");
	printf("Slice: %d\n\n", SLICE_STATS_V0);
	// returns a handle
	sm_ans_xapp_t report_handle_control = report_sm_xapp_api(&nodes.n[0].id, nodes.n[0].ack_rf[0].id, inter, &sm_cb_slice);

	sleep(10);
	// Control DEL slice
    sm_ag_if_wr_t ctrl_msg_del = fill_slice_sm_ctrl_req(SM_SLICE_ID, SLICE_CTRL_SM_V0_DEL);
    control_sm_xapp_api(&nodes.n[0].id, SM_SLICE_ID, &ctrl_msg_del);
    free_slice_ctrl_msg(&ctrl_msg_del.slice_req_ctrl.msg);
	sleep(20);
	// Remove the handle previously returned
	rm_report_sm_xapp_api(report_handle_control.u.handle);


	while(try_stop_xapp_api()==false){
		usleep(1000);
	}

	// Free nodes memory
	free_e2_node_arr(&nodes);


	printf("*****End xAPP*****\n");
	return 0;
}

/*
void init_xapp_api(fr_args_t const*);
  
bool try_stop_xapp_api(void);     

e2_node_arr_t e2_nodes_xapp_api(void);

typedef void (*sm_cb)(sm_ag_if_rd_t const*);

typedef union{
	  char* reason;
	    int handle;
} sm_ans_xapp_u;

typedef struct{
	  sm_ans_xapp_u u;
	    bool success;
} sm_ans_xapp_t;

typedef enum{
	  ms_1,
	    ms_2,
	      ms_5,
	        ms_10,


		  ms_end,
} inter_xapp_e;

// returns a handle
sm_ans_xapp_t report_sm_xapp_api (global_e2_node_id_t* id, uint32_t sm_id, inter_xapp_e i, sm_cb handler);

// Remove the handle previously returned
void rm_report_sm_xapp_api(int const handle);

// Send control message
sm_ans_xapp_t control_sm_xapp_api(global_e2_node_id_t* id, uint32_t ran_func_id, sm_ag_if_wr_t const* wr);
*/


//e2_node_arr_t -> n (e2_node_connected_t), len
//e2_node_connected_t -> id, ack_rf (RAN Function)
//ack_rf -> id, rev (Revision), def (Definition), oid
