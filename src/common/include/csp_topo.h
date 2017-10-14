/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * (C) 2016 by Argonne National Laboratory.
 *     See COPYRIGHT in top-level directory.
 */
#ifndef TOPO_H_INCLUDED
#define TOPO_H_INCLUDED

#ifdef HAVE_HWLOC
#include <hwloc.h>
#endif

typedef enum CSP_topo_domain_type {
    CSP_TOPO_DOMAIN_MACHINE,
    CSP_TOPO_DOMAIN_NUMA,
    CSP_TOPO_DOMAIN_SOCK,
} CSP_topo_domain_type_t;

extern int CSP_topo_remap(void);

#endif /* TOPO_H_INCLUDED */
