/**
 * Copyright (C) 2019 - ETH Zurich
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/
#define _GNU_SOURCE // Needed for dlnext Needs to be at first line
#include <dlfcn.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mpi.h>
#include <papi.h>
#include "dmapp.h"
#include <stdio.h>
#include <math.h>
#include "awr.h"

#define REMOVE_SRIN
//#define AWR_DEBUG

static uint8_t awr_routing_change_required = 0;
static uint16_t awr_routing_mode = GNI_DLVMODE_ADAPTIVE0;
static uint8_t dmapp_routing_change_required = 0;
static gni_nic_handle_t gni_nics[10000];
static uint16_t gni_nic_next_id = 0;
static int64_t cutoff_value = 4096;
static int routing_low_latency = GNI_DLVMODE_ADAPTIVE3;
static int routing_low_stalls = GNI_DLVMODE_ADAPTIVE0;

static int store_sr = 0;
static double sr_storage[16384];
static int next_sr_storage = 0;
static uint32_t sizes[16384];
static double stalls_old[16384];

static double latencies[16384];
static gni_ep_handle_t eps[16384];
static uint16_t routings[16384];

static int eventset;
#ifdef REMOVE_SRIN
static long long values[5];
#else
static long long values[4];
#endif
static int initialized = 0;
static unsigned long long total_size = 0;
static unsigned long long ad0_size = 0;
static unsigned long long skipped_size = 0;

//static int has_mpi = 0;
static int disabled = 0;
static int eventset_started = 0;
static uint16_t last_routing = 65535, last_routing_true = 65535;
static uint8_t first_iteration = 1;
static double last_sr = -100, last_latency = -100;

// Freshness stuff
static int max_freshness = 5;
static int freshness_ad0 = 0;
static int freshness_ad3 = 0;
static double sr_ad0 = 0;
static double sr_ad3 = 0;
static double lat_ad0 = 0;
static double lat_ad3 = 0;
static gni_ep_handle_t ep_ad0, ep_ad3;
static long size_ad0, size_ad3;
static long forced_switch_every = 10;
static long forced_switch_remaining = 10;


#ifdef REMOVE_SRIN
static char* counters_names[5] = {
  "AR_NIC_NETMON_ORB_EVENT_CNTR_REQ_FLITS",
  "AR_NIC_NETMON_ORB_EVENT_CNTR_REQ_STALLED",
  "AR_NIC_ORB_PRF_NET_RSP_TRACK_2",
  "AR_NIC_NETMON_ORB_EVENT_CNTR_RSP_NET_TRACK",
  "AR_NIC_NETMON_ORB_EVENT_CNTR_RSP_STALLED"
};
#else
static char* counters_names[4] = {
  "AR_NIC_NETMON_ORB_EVENT_CNTR_REQ_FLITS",
  "AR_NIC_NETMON_ORB_EVENT_CNTR_REQ_STALLED",
  "AR_NIC_ORB_PRF_NET_RSP_TRACK_2",
  "AR_NIC_NETMON_ORB_EVENT_CNTR_RSP_NET_TRACK"
};
#endif

static char hostname[1024];
static void finalize(){
  if(!disabled){
    printf("%s total size %f\n", hostname, (double) total_size);
    if(total_size){
      printf("%s AD0 size %f\n", hostname, (double) ad0_size/total_size);
      printf("%s Skipped size %f\n", hostname, (double) skipped_size/total_size);
    }
    if(store_sr){
      printf("SR Storage:\n");
      size_t i;
      for(i = 0; i <  next_sr_storage; i++){
        printf("%s: %d %f %f %f %p Routing: %d\n", hostname, sizes[i], stalls_old[i], sr_storage[i], latencies[i], eps[i], routings[i]);
      }
    }
  }
}

typedef int (*orig_gni_postfma)(gni_ep_handle_t ep_hndl, gni_post_descriptor_t *post_descr);
typedef int (*orig_gni_postrdma)(gni_ep_handle_t ep_hndl, gni_post_descriptor_t *post_descr);
typedef int (*orig_gni_postcqwrite)(gni_ep_handle_t ep_hndl, gni_post_descriptor_t *post_descr);
typedef int (*orig_gni_ctpostfma)(gni_ep_handle_t ep_hndl, gni_post_descriptor_t *ct_post_descr);
typedef int (*orig_gni_smsgsendwtag)(gni_ep_handle_t ep_hndl, void *header, uint32_t header_length, void *data, uint32_t data_length, uint32_t msg_id, uint8_t tag);
typedef int (*orig_gni_smsgsend)(gni_ep_handle_t ep_handle, void *header, uint32_t header_length, void *data, uint32_t data_length, uint32_t msg_id);
typedef int (*orig_gni_cdmattach)(gni_cdm_handle_t cdm_handle, uint32_t device_id, uint32_t *local_address, gni_nic_handle_t *nic_handle);
typedef int (*orig_gni_cdmcreate)(uint32_t inst_id, uint8_t ptag, uint32_t cookie, uint32_t modes, gni_cdm_handle_t *cdm_handle);

orig_gni_cdmcreate orig_cdmcreate;
orig_gni_postfma orig_postfma;
orig_gni_postrdma orig_postrdma;
orig_gni_postcqwrite orig_postcqwrite;
orig_gni_ctpostfma orig_ctpostfma;
orig_gni_smsgsendwtag orig_smsgsendwtag;
orig_gni_smsgsend orig_smsgsend;
orig_gni_cdmattach orig_cdmattach;


static void init(void){
  if(!initialized){
    char* disable = getenv("AWR_ROUTING_DISABLE");
    if(disable){
      disabled = atoi(disable);
    }

    char* rtype = getenv("AWR_ROUTING_TYPE");
    if(rtype){
      awr_change_routing(atoi(rtype));
    }

    char* cutoff = getenv("AWR_CUTOFF");
    if(cutoff){
      cutoff_value = atoi(cutoff);
    }
    
    char* storesr = getenv("AWR_STORE_SR");
    if(storesr){
      store_sr = atoi(storesr);
    }

    char* rll = getenv("AWR_ROUTING_LOW_LATENCY");
    if(rll){
      routing_low_latency = atoi(rll);
    }

    char* rls = getenv("AWR_ROUTING_LOW_STALLS");
    if(rls){
      routing_low_stalls = atoi(rls);
    }


    gethostname(hostname, 1024);
    if(!disabled){
      if(1 /*PAPI_is_initialized() == PAPI_NOT_INITED*/){
        int i = PAPI_library_init(PAPI_VER_CURRENT);
        if(i != PAPI_VER_CURRENT ) {
          printf("PAPI initialization error (%d vs %d)! \n", i, PAPI_VER_CURRENT);
          exit(1);
        }
      }
      eventset = PAPI_NULL;
      if(PAPI_create_eventset(&eventset) != PAPI_OK){
        printf("Failed to create eventset\n");
        exit(11);
      }

      size_t num_counters = sizeof(counters_names) / sizeof(counters_names[0]);
      size_t i;
      for(i = 0; i < num_counters; i++){
        int papi_code;
        PAPI_event_name_to_code(counters_names[i], &papi_code);
        int rr;
        if((rr = PAPI_add_event(eventset, papi_code)) != PAPI_OK) {
          char p1[1024];
          PAPI_event_code_to_name(papi_code, p1);
          fprintf(stderr, "Adding PAPI counter %ld (%s) failed with %d!\n", i, p1, rr);
          exit(11);
        }
      }
      PAPI_start(eventset);
      eventset_started = 1;
      orig_cdmcreate = (orig_gni_cdmcreate) dlsym(RTLD_NEXT,"GNI_CdmCreate");
      orig_postfma = (orig_gni_postfma) dlsym(RTLD_NEXT,"GNI_PostFma");
      orig_postrdma = (orig_gni_postrdma) dlsym(RTLD_NEXT,"GNI_PostRdma");
      orig_postcqwrite = (orig_gni_postcqwrite) dlsym(RTLD_NEXT,"GNI_PostCqWrite");
      orig_ctpostfma = (orig_gni_ctpostfma) dlsym(RTLD_NEXT,"GNI_CtPostFma");
      orig_smsgsendwtag = (orig_gni_smsgsendwtag) dlsym(RTLD_NEXT,"GNI_SmsgSendWTag");
      orig_smsgsend = (orig_gni_smsgsend) dlsym(RTLD_NEXT,"GNI_SmsgSend");
      orig_cdmattach = (orig_gni_cdmattach) dlsym(RTLD_NEXT,"GNI_CdmAttach");
    }
    initialized = 1;
    atexit(finalize);
  }
}


static uint16_t get_next_routing(double size, double sr, double latency, gni_post_type_t type, gni_ep_handle_t ep){
  double num_flits = size / (64/5.0);
  switch(type){
  case GNI_POST_RDMA_GET:
  case GNI_POST_FMA_GET:
    num_flits /= 5.0; // In Gets data is carried by responses. We have approximately 1/5 of requests flits.
    break;
  default:
    break;
  }

  double factorlat = 1.2;
  double factorstalls = 5.0;

  double cycles_to_microsecs = 1.25 / 1000.0;

  if(sr < 0){
    sr = 0; // Since we are removing sr_in, in some cases due to skew in reading the two counters we may have sr_in higher than sr_out. This could lead to an sr value slightly lower than 0
  }
  
  double stalls_ms = sr*cycles_to_microsecs;
  double factor = 0;
  double estimated_latency = 0;
  double estimated_sr = 0;
  double num_packets = ceil(size / 64.0);
  //double batches = ceil(num_packets / 1024.0); // Number of "batches"
  //double alpha = batches;
  double alpha = ceil((num_packets - 1 + 512) / 1024.0);

  if(last_routing_true == routing_low_latency){
    freshness_ad3 = max_freshness;
    sr_ad3 = sr;
    lat_ad3 = latency;
    ep_ad3 = ep;
    size_ad3 = size;

    --freshness_ad0;

    /*
   if(forced_switch_remaining-- <= 0){
     return routing_low_stalls;
   }
   */

    // NEW
    if(freshness_ad0 > 0 && ep == ep_ad0 && size == size_ad0){
      estimated_latency = lat_ad0;
      estimated_sr = sr_ad0 * cycles_to_microsecs;
    }else{
      estimated_latency = factorlat*latency;
      estimated_sr = (1.0/factorstalls)*stalls_ms;
    }
    /*
   if(stalls_ms - estimated_sr <= 0 || (estimated_latency - latency <= 0)){
     return routing_low_latency;
   }
   */

    factor = (alpha*(estimated_latency - latency)) / (stalls_ms - estimated_sr);
    if(num_flits > factor){
      last_routing_true = routing_low_stalls;
      return routing_low_stalls;
    }else{
      last_routing_true = routing_low_latency;
      return routing_low_latency;
    }
  }else{
    // AD0
    freshness_ad0 = max_freshness;
    sr_ad0 = sr;
    lat_ad0 = latency;
    ep_ad0 = ep;
    size_ad0 = size;

    --freshness_ad3;

    if(freshness_ad3 > 0  && ep == ep_ad3 && size == size_ad3){
      estimated_latency = lat_ad3;
      estimated_sr = sr_ad3 * cycles_to_microsecs;
    }else{
      estimated_latency = (1.0/factorlat)*latency;
      estimated_sr = factorstalls*stalls_ms;
    }

    factor = (alpha*(latency - estimated_latency)) / (estimated_sr - stalls_ms);
    if(num_flits < factor){
      last_routing_true = routing_low_latency;
      return routing_low_latency;
    }else{
      last_routing_true = routing_low_stalls;
      return routing_low_stalls;
    }
  }
}

void awr_enable(){
  disabled = 0;
  if(!eventset_started){
    PAPI_start(eventset);
    eventset_started = 1;
  }
  PAPI_read(eventset, values);
  first_iteration = 1;
  awr_change_routing(last_routing);
}

void awr_disable(){
  disabled = 1;
  if(eventset_started){
#ifdef REMOVE_SRIN
    long long tmpvalues[5];
#else
    long long tmpvalues[4];
#endif
    PAPI_stop(eventset, tmpvalues);

    last_latency = (double) (tmpvalues[2] - values[2]) / (tmpvalues[3] - values[3]);
#ifdef REMOVE_SRIN
    last_sr      = (double) ((tmpvalues[1] - tmpvalues[4]) - (values[1] - values[4])) / (tmpvalues[0] - values[0]);
#else
    last_sr      = (double) ((tmpvalues[1]) - (values[1])) / (tmpvalues[0] - values[0]);
#endif
    eventset_started = 0;
  }
  dmapp_routing_change_required = 0;
  awr_routing_change_required = 0;
}

void awr_change_routing(uint16_t routing){
  awr_routing_mode = routing;
  dmapp_routing_change_required = 1;
  awr_routing_change_required = 1;
  uint16_t i;
  for(i = 0; i < gni_nic_next_id; i++){
    GNI_SmsgSetDeliveryMode(gni_nics[i], routing);
  }
}

static void papi_pre(uint32_t size, gni_ep_handle_t ep, gni_post_type_t type){
  uint16_t next_routing = last_routing;
  static uint64_t cumulative_size = 0;
  cumulative_size += size;

  if(cumulative_size <= cutoff_value){
    next_routing = routing_low_latency;
    skipped_size += size;
  }else if(last_sr != -100 && last_latency != -100){
    cumulative_size = 0;
    next_routing = get_next_routing(size, last_sr, last_latency, type, ep);

    if(store_sr && next_sr_storage < 16384){
      sizes[next_sr_storage] = size;
      sr_storage[next_sr_storage] = last_sr;
      latencies[next_sr_storage] = last_latency;
      eps[next_sr_storage] = ep;
      routings[next_sr_storage] = next_routing;
      next_sr_storage++;
    }
  }

  if(next_routing != last_routing){
    forced_switch_remaining = forced_switch_every;
    awr_change_routing(next_routing);
  }

  last_routing = next_routing;

  total_size += size;
  if(last_routing == routing_low_stalls){
    ad0_size += size;
  }
}

static void update_counters(uint32_t size){
  static uint64_t cumulative_size = 0;
  cumulative_size += size;

  if(cumulative_size <= cutoff_value){
    return;
  }else{
    cumulative_size = 0;
  }

#ifdef REMOVE_SRIN
  long long tmpvalues[5];
#else
  long long tmpvalues[4];
#endif
  PAPI_read(eventset, tmpvalues);

  double sr = -1;
  double latency = -1;
  if(tmpvalues[3] > values[3] &&
     tmpvalues[0] > values[0] &&
     !first_iteration){
    latency = (double) (tmpvalues[2] - values[2]) / (tmpvalues[3] - values[3]);
#ifdef REMOVE_SRIN
    sr      = (double) ((tmpvalues[1] - tmpvalues[4]) - (values[1] - values[4])) / (tmpvalues[0] - values[0]);
#else
    sr      = (double) ((tmpvalues[1]) - (values[1])) / (tmpvalues[0] - values[0]);
#endif
  }else{
    latency = last_latency;
    sr = last_sr;
    first_iteration = 0;
  }

  last_latency = latency;
  last_sr = sr;

  values[0] = tmpvalues[0];
  values[1] = tmpvalues[1];
  values[2] = tmpvalues[2];
  values[3] = tmpvalues[3];
#ifdef REMOVE_SRIN
  values[4] = tmpvalues[4];
#endif
}

gni_return_t GNI_PostFma(gni_ep_handle_t ep_hndl, gni_post_descriptor_t *post_descr){
  if(!initialized){init();}
  if(!disabled){
    papi_pre(post_descr->length, ep_hndl, post_descr->type);
  }
#ifdef AWR_DEBUG
  printf("PostFma of size %d to ep %p\n", post_descr->length, ep_hndl);
#endif
  if(post_descr && awr_routing_change_required){
    post_descr->dlvr_mode = awr_routing_mode;
  }

  gni_return_t r = orig_postfma(ep_hndl, post_descr);
  if(!disabled){
    update_counters(post_descr->length);
  }
  return r;
}


gni_return_t GNI_PostRdma(gni_ep_handle_t ep_hndl, gni_post_descriptor_t *post_descr){
  if(!initialized){init();}
  if(!disabled){
    papi_pre(post_descr->length, ep_hndl, post_descr->type);
  }
#ifdef AWR_DEBUG
  printf("PostRdma of size %d to ep %p\n", post_descr->length, ep_hndl);
#endif

  if(post_descr && awr_routing_change_required){
    post_descr->dlvr_mode = awr_routing_mode;
  }
  gni_return_t r = orig_postrdma(ep_hndl, post_descr);
  if(!disabled){
    update_counters(post_descr->length);
  }
  return r;
}


gni_return_t GNI_PostCqWrite(gni_ep_handle_t ep_hndl, gni_post_descriptor_t *post_descr){
#ifdef AWR_DEBUG
  printf("PostCqWrite of size %d to ep %p\n", post_descr->length, ep_hndl);
#endif
  if(!initialized){init();}
  if(!disabled){
    papi_pre(post_descr->length, ep_hndl, post_descr->type);
  }
  if(post_descr && awr_routing_change_required){
    post_descr->dlvr_mode = awr_routing_mode;
  }
  gni_return_t r = orig_postcqwrite(ep_hndl, post_descr);
  if(!disabled){
    update_counters(post_descr->length);
  }
  return r;
}

gni_return_t GNI_CtPostFma(gni_ep_handle_t ep_hndl, gni_post_descriptor_t *ct_post_descr){
#ifdef AWR_DEBUG
  printf("CtPostFma of size %d to ep %p\n", ct_post_descr->length, ep_hndl);
#endif
  if(!initialized){init();}
  if(!disabled){
    papi_pre(ct_post_descr->length, ep_hndl, ct_post_descr->type);
  }
  if(ct_post_descr && awr_routing_change_required){
    ct_post_descr[0].dlvr_mode = awr_routing_mode;
  }
  gni_return_t r = orig_ctpostfma(ep_hndl, ct_post_descr);
  if(!disabled){
    update_counters(ct_post_descr->length);
  }
  return r;
}

gni_return_t GNI_SmsgSendWTag(gni_ep_handle_t ep_hndl, void *header, uint32_t header_length, void *data, uint32_t data_length, uint32_t msg_id, uint8_t tag){
#ifdef AWR_DEBUG
  printf("SmsgSendWTag of size %d to ep %p\n", data_length, ep_hndl);
#endif
  if(!initialized){init();}
  if(!disabled){
    //papi_pre(data_length, ep_hndl, GNI_POST_FMA_PUT);
    awr_change_routing(routing_low_latency);
    last_routing = routing_low_latency;
  }

  gni_return_t r = orig_smsgsendwtag(ep_hndl, header, header_length, data, data_length, msg_id, tag);
  if(!disabled){
    //update_counters(data_length);
  }
  return r;
}

gni_return_t GNI_SmsgSend (gni_ep_handle_t ep_handle, void *header, uint32_t header_length, void *data, uint32_t data_length, uint32_t msg_id){
#ifdef AWR_DEBUG
  printf("SmsgSend of size %d to ep %p\n", data_length, ep_handle);
#endif
  if(!initialized){init();}
  if(!disabled){
    //papi_pre(data_length, ep_handle, GNI_POST_FMA_PUT);
    awr_change_routing(routing_low_latency);
    last_routing = routing_low_latency;
  }
  gni_return_t r = orig_smsgsend(ep_handle, header, header_length, data, data_length, msg_id);
  if(!disabled){
    //update_counters(data_length);
  }
  return r;
}

gni_return_t GNI_CdmAttach(gni_cdm_handle_t cdm_handle, uint32_t device_id, uint32_t *local_address, gni_nic_handle_t *nic_handle){
#ifdef AWR_DEBUG
  printf("CdmAttach called\n");
#endif
  if(!initialized){init();}
  gni_return_t r = orig_cdmattach(cdm_handle, device_id, local_address, nic_handle);
  gni_nics[gni_nic_next_id++] = *nic_handle;
  return r;
}

gni_return_t GNI_CdmCreate(uint32_t inst_id, uint8_t ptag, uint32_t cookie, uint32_t modes, gni_cdm_handle_t *cdm_handle){
  if(!initialized){init();}
  return orig_cdmcreate(inst_id, ptag, cookie, modes, cdm_handle);
}

/*
static void dmapp_change_routing(){
  uint8_t dmapp_routing = DMAPP_ROUTING_ADAPTIVE;
  switch(awr_routing_mode){
  case GNI_DLVMODE_IN_ORDER:{
    dmapp_routing = DMAPP_ROUTING_IN_ORDER;
  }break;
  case GNI_DLVMODE_NMIN_HASH:{
    dmapp_routing = DMAPP_ROUTING_DETERMINISTIC;
  }break;
  case GNI_DLVMODE_MIN_HASH:{
    dmapp_routing = DMAPP_ROUTING_DETERMINISTIC_1;
  }break;
  case GNI_DLVMODE_ADAPTIVE0:{
    dmapp_routing = DMAPP_ROUTING_ADAPTIVE;
  }break;
  case GNI_DLVMODE_ADAPTIVE1:{
    dmapp_routing = DMAPP_ROUTING_ADAPTIVE_1;
  }break;
  case GNI_DLVMODE_ADAPTIVE2:{
    dmapp_routing = DMAPP_ROUTING_ADAPTIVE_2;
  }break;
  case GNI_DLVMODE_ADAPTIVE3:{
    dmapp_routing = DMAPP_ROUTING_ADAPTIVE_3;
  }break;
  default:{};
  }

  dmapp_rma_attrs_ext_t attrs, attrs_old;
  if(dmapp_get_rma_attrs_ext(&attrs) != DMAPP_RC_SUCCESS){
    fprintf(stderr, "dmapp_get_rma_attrs_ext failed\n");
    exit(1);
  }
  attrs.put_relaxed_ordering = dmapp_routing;
  attrs.get_relaxed_ordering = dmapp_routing;
  if(dmapp_set_rma_attrs_ext(&attrs, &attrs_old) != DMAPP_RC_SUCCESS){
    fprintf(stderr, "dmapp_set_rma_attrs_ext failed\n");
    exit(1);
  }
}
*/

int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm){
  int ret;
  // Use ADAPTIVE1 instead of ADAPTIVE0, as is done by default in Cray Aries.
  routing_low_stalls = GNI_DLVMODE_ADAPTIVE1;
  ret = PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
  routing_low_stalls = GNI_DLVMODE_ADAPTIVE0;
  return ret;
}
