/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Parallel tests for subfiling feature
 */

#include "testphdf5.h"

/* FILENAME and filenames must have the same number of names.
 * Use PARATESTFILE in general and use a separated filename only if the file
 * created in one test is accessed by a different test.
 * filenames[0] is reserved as the file name for PARATESTFILE.
 */
#define NFILENAME 2
#define PARATESTFILE filenames[0]
const char *FILENAME[NFILENAME]={
	    "SubFileTest",
	    NULL};
char	filenames[NFILENAME][PATH_MAX];

int nerrors = 0;			/* errors count */
hid_t	fapl;				/* file access property list */

#define DATASET1      "Dataset1_subfiled"
#define RANK          2
#define DIMS0         6
#define DIMS1         120

void subf_api(void);
void subf_fpp_w(void);
void subf_fpp_r(void);
void subf_2_w(void);
void subf_2_r(void);

void
subf_api(void)
{
    hid_t fid;                  /* HDF5 file ID */
    hid_t did;
    hid_t sid;
    hid_t fapl_id, dapl_id;	/* Property Lists */

    const char *filename;
    char subfile_name[50];
    hsize_t dims[RANK];   	/* dataset dim sizes */
    hsize_t start[RANK], count[RANK], stride[RANK], block[RANK];

    int mpi_size, mpi_rank;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Info info = MPI_INFO_NULL;

    herr_t ret;         	/* Generic return value */

    filename = (const char *)GetTestParameters();

    /* set up MPI parameters */
    MPI_Comm_size(MPI_COMM_WORLD,&mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD,&mpi_rank);

    /* create file access property list */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    VRFY((fapl_id >= 0), "");

    ret = H5Pset_fapl_mpio(fapl_id, comm, info);
    VRFY((ret == 0), "");

    /* create the dataspace for the master dataset */
    dims[0] = DIMS0 * mpi_size;
    dims[1] = DIMS1;
    sid = H5Screate_simple (2, dims, NULL);
    VRFY((sid >= 0), "");

    /* set the selection for this dataset that this process will write to */
    start[0] = DIMS0 * mpi_rank;
    start[1] = 0;
    block[0] = DIMS0;
    block[1] = DIMS1;
    count[0] = 1;
    count[1] = 1;
    stride[0] = 1;
    stride[1] = 1;
    ret = H5Sselect_hyperslab(sid, H5S_SELECT_SET, start, stride, count, block);
    VRFY((ret == 0), "H5Sset_hyperslab succeeded");
    /* create the dataset access property list */
    dapl_id = H5Pcreate(H5P_DATASET_ACCESS);
    VRFY((dapl_id >= 0), "");
    /* Set the selection that this process will access the dataset with */
    ret = H5Pset_subfiling_selection(dapl_id, sid);
    VRFY((ret == 0), "");

    /* create a file with no subfiling enabled */
    fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);
    VRFY((fid >= 0), "H5Fcreate succeeded");
    /* try to create a dataset with subfiling enabled - should fail */
    H5E_BEGIN_TRY {
        did = H5Dcreate2(fid, DATASET1, H5T_NATIVE_INT, sid, H5P_DEFAULT, H5P_DEFAULT, dapl_id);
    } H5E_END_TRY;
    VRFY((did < 0), "H5Dcreate failed");
    /* close the file */
    ret = H5Fclose(fid);
    VRFY((ret == 0), "");

    /* set subfiling to be 1 file per mpi rank */
    /* set name of subfile */
    sprintf(subfile_name, "Subfile_%d.h5", mpi_rank);

    /* set number of process groups to be equal to the mpi size */
    ret = H5Pset_subfiling_access(fapl_id, subfile_name, MPI_COMM_SELF, MPI_INFO_NULL);
    VRFY((ret == 0), "H5Pset_subfiling_access succeeded");

    /* create the file. This should also create the subfiles */
    fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);
    VRFY((fid >= 0), "H5Fcreate succeeded");

    /* close the file and re-open it with the same access property list */
    ret = H5Fclose(fid);
    VRFY((ret == 0), "");
    fid = H5Fopen(filename, H5F_ACC_RDWR, fapl_id);
    VRFY((fid >= 0), "H5Fopen succeeded");

    /* close the access plist and reopen to check the property values */
    ret = H5Pclose(fapl_id);
    VRFY((ret == 0), "");

    /* check the file access properties */
    fapl_id = H5Fget_access_plist(fid);
    VRFY((fapl_id >= 0), "");
    {
        MPI_Comm get_comm;
        MPI_Info get_info;
        int comm_size;
        char *temp_name = NULL;

        ret = H5Pget_subfiling_access(fapl_id, &temp_name, &get_comm, &get_info);
        VRFY((ret == 0), "");

        VRFY((strcmp(temp_name, subfile_name) == 0), "Subfile name verification succeeded");

        MPI_Comm_size(get_comm, &comm_size);
        VRFY((comm_size == 1), "subfiling communicator verified");

        HDfree(temp_name);
    }

    ret = H5Pclose(fapl_id);
    VRFY((ret == 0), "");

    /* create the dataset with the subfiling dapl settings */
    did = H5Dcreate2(fid, DATASET1, H5T_NATIVE_INT, sid, H5P_DEFAULT, H5P_DEFAULT, dapl_id);
    VRFY((did >= 0), "");

    /* check the dataset creation properties for this process */
    {
        hid_t temp_sid;

        ret = H5Pget_subfiling_selection(dapl_id, &temp_sid);
        VRFY((ret == 0), "");

        VRFY((H5Sget_select_npoints(sid) == H5Sget_select_npoints(temp_sid)), "Subfiling selection verified");
        VRFY((H5Sget_simple_extent_ndims(sid) == H5Sget_simple_extent_ndims(temp_sid)), "Subfiling selection verified");

        ret = H5Sclose(temp_sid);
        VRFY((ret == 0), "");
    }

    ret = H5Pclose(dapl_id);
    VRFY((ret == 0), "");

    ret = H5Dclose(did);
    VRFY((ret == 0), "");

    ret = H5Sclose(sid);
    VRFY((ret == 0), "");

    ret = H5Fclose(fid);
    VRFY((ret == 0), "");

    MPI_File_delete(subfile_name, MPI_INFO_NULL);

    return;
} /* subf_api */

void
subf_fpp_w(void)
{
    hid_t fid;                  /* HDF5 file ID */
    hid_t did;
    hid_t sid, mem_space_id;
    hid_t fapl_id, dapl_id;	/* Property Lists */

    const char *filename;
    char subfile_name[50];
    hsize_t dims[RANK];   	/* dataset dim sizes */
    hsize_t npoints, start[RANK], count[RANK], stride[RANK], block[RANK];

    int mpi_size, mpi_rank, i;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Info info = MPI_INFO_NULL;

    int *wbuf;
    herr_t ret;         	/* Generic return value */

    filename = (const char *)GetTestParameters();

    /* set up MPI parameters */
    MPI_Comm_size(MPI_COMM_WORLD,&mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD,&mpi_rank);

    /* create file access property list */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    VRFY((fapl_id >= 0), "");

    ret = H5Pset_fapl_mpio(fapl_id, comm, info);
    VRFY((ret == 0), "");

    /* set subfiling to be 1 file per mpi rank */

    /* set name of subfile */
    sprintf(subfile_name, "Subfile_%d.h5", mpi_rank);

    /* set number of process groups to be equal to the mpi size */
    ret = H5Pset_subfiling_access(fapl_id, subfile_name, MPI_COMM_SELF, MPI_INFO_NULL);
    VRFY((ret == 0), "H5Pset_subfiling_access succeeded");

    /* create the file. This should also create the subfiles */
    fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);
    VRFY((fid >= 0), "H5Fcreate succeeded");

    ret = H5Pclose(fapl_id);
    VRFY((ret == 0), "");

    dims[0] = DIMS0 * mpi_size;
    dims[1] = DIMS1;

    /* create the dataspace for the master dataset */
    sid = H5Screate_simple (2, dims, NULL);
    VRFY((sid >= 0), "");

    start[0] = DIMS0 * mpi_rank;
    start[1] = 0;
    block[0] = DIMS0;
    block[1] = DIMS1;
    count[0] = 1;
    count[1] = 1;
    stride[0] = 1;
    stride[1] = 1;

    /* set the selection for this dataset that this process will write to */
    ret = H5Sselect_hyperslab(sid, H5S_SELECT_SET, start, stride, count, block);
    VRFY((ret == 0), "H5Sset_hyperslab succeeded");

    /* create the dataset access property list */
    dapl_id = H5Pcreate(H5P_DATASET_ACCESS);
    VRFY((dapl_id >= 0), "");

    /* Set the selection that this process will access the dataset with */
    ret = H5Pset_subfiling_selection(dapl_id, sid);
    VRFY((ret == 0), "");

    /* create the dataset with the subfiling dapl settings */
    did = H5Dcreate2(fid, DATASET1, H5T_NATIVE_INT, sid, H5P_DEFAULT, H5P_DEFAULT, dapl_id);
    VRFY((did >= 0), "");

    ret = H5Pclose(dapl_id);
    VRFY((ret == 0), "");

    npoints = DIMS0 * DIMS1;
    wbuf = (int *)HDmalloc(npoints * sizeof(int));

    for(i=0 ; i<npoints ; i++)
        wbuf[i] = (mpi_rank+1) * 10;

    HDassert(npoints == H5Sget_select_npoints(sid));

    mem_space_id = H5Screate_simple(1, &npoints, NULL);
    VRFY((mem_space_id >= 0), "");

    ret = H5Dwrite(did, H5T_NATIVE_INT, mem_space_id, sid, H5P_DEFAULT, wbuf);
    if(ret < 0) FAIL_STACK_ERROR;
    VRFY((ret == 0), "");

    ret = H5Dclose(did);
    VRFY((ret == 0), "");

    ret = H5Sclose(sid);
    VRFY((ret == 0), "");

    ret = H5Sclose(mem_space_id);
    VRFY((ret == 0), "");

    ret = H5Fclose(fid);
    VRFY((ret == 0), "");

    HDfree(wbuf);

    return;
error:
    VRFY((1 == 0), "");
} /* subf_fpp_w */

void
subf_fpp_r(void)
{
    hid_t fid;                  /* HDF5 file ID */
    hid_t did;
    hid_t sid, mem_space_id;
    hid_t fapl_id, dxpl_id;  	/* Property Lists */

    const char *filename;
    char subfile_name[50];
    hsize_t npoints, start[RANK], count[RANK], stride[RANK], block[RANK];

    int mpi_size, mpi_rank, i;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Info info = MPI_INFO_NULL;

    int *rbuf;
    herr_t ret;         	/* Generic return value */

    filename = (const char *)GetTestParameters();

    /* set up MPI parameters */
    MPI_Comm_size(MPI_COMM_WORLD,&mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD,&mpi_rank);

    /* create file access property list */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    VRFY((fapl_id >= 0), "");

    ret = H5Pset_fapl_mpio(fapl_id, comm, info);
    VRFY((ret == 0), "");

    /* set subfiling to be 1 file per mpi rank */

    /* set name of subfile */
    sprintf(subfile_name, "Subfile_%d.h5", mpi_rank);

    /* set number of process groups to be equal to the mpi size */
    ret = H5Pset_subfiling_access(fapl_id, subfile_name, MPI_COMM_SELF, MPI_INFO_NULL);
    VRFY((ret == 0), "H5Pset_subfiling_access succeeded");

    /* open the file */
    fid = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id);
    VRFY((fid >= 0), "H5Fopen succeeded");

    ret = H5Pclose(fapl_id);
    VRFY((ret == 0), "");

    /* open the dataset */
    did = H5Dopen2(fid, DATASET1, H5P_DEFAULT);
    VRFY((did >= 0), "");

    /* create the dataspace for the master dataset */
    sid = H5Dget_space(did);
    VRFY((sid >= 0), "");

    start[0] = DIMS0 * mpi_rank;
    start[1] = 0;
    block[0] = DIMS0;
    block[1] = DIMS1;
    count[0] = 1;
    count[1] = 1;
    stride[0] = 1;
    stride[1] = 1;

    /* set the selection for this dataset that this process will read from */
    ret = H5Sselect_hyperslab(sid, H5S_SELECT_SET, start, stride, count, block);
    VRFY((ret == 0), "H5Sset_hyperslab succeeded");

    npoints = DIMS0 * DIMS1;
    rbuf = (int *)HDmalloc(npoints * mpi_size * sizeof(int));

    mem_space_id = H5Screate_simple(1, &npoints, NULL);
    VRFY((mem_space_id >= 0), "");

    ret = H5Dread(did, H5T_NATIVE_INT, mem_space_id, sid, H5P_DEFAULT, rbuf);
    if(ret < 0) FAIL_STACK_ERROR;
    VRFY((ret == 0), "");

    for(i=0 ; i<npoints ; i++)
        VRFY((rbuf[i] == (mpi_rank+1) * 10), "Data read verified");

    ret = H5Dclose(did);
    VRFY((ret == 0), "");

    ret = H5Sclose(sid);
    VRFY((ret == 0), "");

    ret = H5Sclose(mem_space_id);
    VRFY((ret == 0), "");

    ret = H5Fclose(fid);
    VRFY((ret == 0), "");

    /* try to read with no subfiling on the opened file */

    /* create file access property list */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    VRFY((fapl_id >= 0), "");
    ret = H5Pset_fapl_mpio(fapl_id, comm, info);
    VRFY((ret == 0), "");
    fid = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id);
    VRFY((fid >= 0), "H5Fopen succeeded");
    ret = H5Pclose(fapl_id);
    VRFY((ret == 0), "");

    /* open the dataset */
    did = H5Dopen2(fid, DATASET1, H5P_DEFAULT);
    VRFY((did >= 0), "");

    /* Create dataset transfer property list */
    dxpl_id = H5Pcreate(H5P_DATASET_XFER);
    VRFY((dxpl_id > 0), "H5Pcreate succeeded");

    /* set collective I/O which should be broken */
    ret = H5Pset_dxpl_mpio(dxpl_id, H5FD_MPIO_COLLECTIVE);
    VRFY((ret >= 0), "H5Pset_dxpl_mpio succeeded");

    {
        hsize_t temp = npoints * mpi_size;

        mem_space_id = H5Screate_simple(1, &temp, NULL);
        VRFY((mem_space_id >= 0), "");
    }

    ret = H5Dread(did, H5T_NATIVE_INT, mem_space_id, H5S_ALL, dxpl_id, rbuf);
    if(ret < 0) FAIL_STACK_ERROR;
    VRFY((ret == 0), "");

    {
        uint32_t local_cause, global_cause;

        H5Pget_mpio_no_collective_cause(dxpl_id, &local_cause, &global_cause);
        VRFY((ret == 0), "");
        VRFY((local_cause == H5D_MPIO_VDS_PARALLEL_READ), "Broke Collective I/O");
        VRFY((global_cause == H5D_MPIO_VDS_PARALLEL_READ), "Broke Collective I/O");
    }

    ret = H5Pclose(dxpl_id);
    VRFY((ret == 0), "");

    for(i=0 ; i<npoints*mpi_size ; i++) {
        int temp = i/npoints;

        VRFY((rbuf[i] == (temp%mpi_size + 1) * 10), "Data read verified");
    }

    ret = H5Dclose(did);
    VRFY((ret == 0), "");

    ret = H5Fclose(fid);
    VRFY((ret == 0), "");

    ret = H5Sclose(mem_space_id);
    VRFY((ret == 0), "");

    HDfree(rbuf);

    MPI_File_delete(subfile_name, MPI_INFO_NULL);

    return;
error:
    VRFY((1 == 0), "");
} /* subf_fpp_r */

void
subf_2_w(void)
{
    hid_t fid;                  /* HDF5 file ID */
    hid_t did;
    hid_t sid, mem_space_id;
    hid_t fapl_id, dapl_id, dxpl_id;	/* Property Lists */

    const char *filename;
    char subfile_name[50];
    hsize_t dims[RANK];   	/* dataset dim sizes */
    hsize_t npoints, start[RANK], count[RANK], stride[RANK], block[RANK];

    int mpi_size, mpi_rank, mrc, color, i;
    MPI_Comm comm;
    MPI_Info info = MPI_INFO_NULL;

    int *wbuf;
    herr_t ret;         	/* Generic return value */

    filename = (const char *)GetTestParameters();

    /* set up MPI parameters */
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    /* create file access property list */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    VRFY((fapl_id >= 0), "");

    ret = H5Pset_fapl_mpio(fapl_id, MPI_COMM_WORLD, info);
    VRFY((ret == 0), "");

    /* set subfiling to be to 2 files */
    color = mpi_rank % 2;

    /* set name of subfile */
    sprintf(subfile_name, "Subfile_%d.h5", color);

    /* splits processes into 2 groups */
    mrc = MPI_Comm_split (MPI_COMM_WORLD, color, mpi_rank, &comm);
    VRFY((mrc==MPI_SUCCESS), "Comm_split succeeded");

    /* set number of process groups to 2 */
    ret = H5Pset_subfiling_access(fapl_id, subfile_name, comm, MPI_INFO_NULL);
    VRFY((ret == 0), "H5Pset_subfiling_access succeeded");

    /* create the file. This should also create the subfiles */
    fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);
    VRFY((fid >= 0), "H5Fcreate succeeded");

    ret = H5Pclose(fapl_id);
    VRFY((ret == 0), "");

    dims[0] = DIMS0 * mpi_size;
    dims[1] = DIMS1;

    /* create the dataspace for the master dataset */
    sid = H5Screate_simple (2, dims, NULL);
    VRFY((sid >= 0), "");

    start[0] = DIMS0 * mpi_rank;
    start[1] = 0;
    block[0] = DIMS0;
    block[1] = DIMS1;
    count[0] = 1;
    count[1] = 1;
    stride[0] = 1;
    stride[1] = 1;

    /* set the selection for this dataset that this process will write to */
    ret = H5Sselect_hyperslab(sid, H5S_SELECT_SET, start, stride, count, block);
    VRFY((ret == 0), "H5Sset_hyperslab succeeded");

    /* create the dataset access property list */
    dapl_id = H5Pcreate(H5P_DATASET_ACCESS);
    VRFY((dapl_id >= 0), "");

    /* Set the selection that this process will access the dataset with */
    ret = H5Pset_subfiling_selection(dapl_id, sid);
    VRFY((ret == 0), "");

    /* create the dataset with the subfiling dapl settings */
    did = H5Dcreate2(fid, DATASET1, H5T_NATIVE_INT, sid, H5P_DEFAULT, H5P_DEFAULT, dapl_id);
    VRFY((did >= 0), "");

    ret = H5Pclose(dapl_id);
    VRFY((ret == 0), "");

    npoints = DIMS0 * DIMS1;
    wbuf = (int *)HDmalloc(npoints * sizeof(int));

    for(i=0 ; i<npoints ; i++)
        wbuf[i] = (mpi_rank+1) * 10;

    HDassert(npoints == H5Sget_select_npoints(sid));

    mem_space_id = H5Screate_simple(1, &npoints, NULL);
    VRFY((mem_space_id >= 0), "");

    /* Create dataset transfer property list */
    dxpl_id = H5Pcreate(H5P_DATASET_XFER);
    VRFY((dxpl_id > 0), "H5Pcreate succeeded");

    ret = H5Pset_dxpl_mpio(dxpl_id, H5FD_MPIO_COLLECTIVE);
    VRFY((ret >= 0), "H5Pset_dxpl_mpio succeeded");

    ret = H5Dwrite(did, H5T_NATIVE_INT, mem_space_id, sid, dxpl_id, wbuf);
    VRFY((ret == 0), "");

    {
        H5D_mpio_actual_io_mode_t io_mode;
        uint32_t local_cause, global_cause;

        ret = H5Pget_mpio_actual_io_mode(dxpl_id, &io_mode);
        VRFY((ret == 0), "");
        VRFY((io_mode == H5D_MPIO_CONTIGUOUS_COLLECTIVE), "Collective Mode invoked");

        H5Pget_mpio_no_collective_cause(dxpl_id, &local_cause, &global_cause);
        VRFY((ret == 0), "");
        VRFY((local_cause == H5D_MPIO_COLLECTIVE), "Collective I/O invoked");
        VRFY((global_cause == H5D_MPIO_COLLECTIVE), "Collective I/O invoked");
    }

    /* check if a process tries to access a different sub-file */
    start[0] = DIMS0 * ((mpi_rank+1) % mpi_size);
    start[1] = 0;
    block[0] = DIMS0;
    block[1] = DIMS1;
    count[0] = 1;
    count[1] = 1;
    stride[0] = 1;
    stride[1] = 1;

    /* set the selection for this dataset that this process will write to */
    ret = H5Sselect_hyperslab(sid, H5S_SELECT_SET, start, stride, count, block);
    VRFY((ret == 0), "H5Sset_hyperslab succeeded");

    /* Should fail sine we try to access wrong selection */
    H5E_BEGIN_TRY {
        ret = H5Dwrite(did, H5T_NATIVE_INT, mem_space_id, sid, dxpl_id, wbuf);
    } H5E_END_TRY;
    VRFY((ret < 0), "H5Dwrite failed");

    ret = H5Pclose(dxpl_id);
    VRFY((ret == 0), "");

    ret = H5Dclose(did);
    VRFY((ret == 0), "");

    ret = H5Sclose(sid);
    VRFY((ret == 0), "");

    ret = H5Sclose(mem_space_id);
    VRFY((ret == 0), "");

    ret = H5Fclose(fid);
    VRFY((ret == 0), "");

    HDfree(wbuf);

    mrc = MPI_Comm_free(&comm);
    VRFY((mrc==MPI_SUCCESS), "MPI_Comm_free");

    return;
error:
    VRFY((1 == 0), "");
} /* subf_2_w */

void
subf_2_r(void)
{
    hid_t fid;                  /* HDF5 file ID */
    hid_t did;
    hid_t sid, mem_space_id, dxpl_id;
    hid_t fapl_id;      	/* Property Lists */

    const char *filename;
    char subfile_name[50];
    hsize_t npoints, start[RANK], count[RANK], stride[RANK], block[RANK];

    int mpi_size, mpi_rank, mrc, color, i;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Info info = MPI_INFO_NULL;

    int *rbuf;
    herr_t ret;         	/* Generic return value */

    filename = (const char *)GetTestParameters();

    /* set up MPI parameters */
    MPI_Comm_size(MPI_COMM_WORLD,&mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD,&mpi_rank);

    /* create file access property list */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    VRFY((fapl_id >= 0), "");

    ret = H5Pset_fapl_mpio(fapl_id, comm, info);
    VRFY((ret == 0), "");

    /* set subfiling to be to 2 files */

    /* set name of subfile */
    sprintf(subfile_name, "Subfile_%d.h5", mpi_rank%2);

    /* splits processes into 2 groups */
    color = mpi_rank % 2;
    mrc = MPI_Comm_split (MPI_COMM_WORLD, color, mpi_rank, &comm);
    VRFY((mrc==MPI_SUCCESS), "Comm_split succeeded");

    /* set number of process groups to 2 */
    ret = H5Pset_subfiling_access(fapl_id, subfile_name, comm, MPI_INFO_NULL);
    VRFY((ret == 0), "H5Pset_subfiling_access succeeded");

    /* open the file */
    fid = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id);
    VRFY((fid >= 0), "H5Fopen succeeded");

    ret = H5Pclose(fapl_id);
    VRFY((ret == 0), "");

    /* open the dataset */
    did = H5Dopen2(fid, DATASET1, H5P_DEFAULT);
    VRFY((did >= 0), "");

    /* create the dataspace for the master dataset */
    sid = H5Dget_space(did);
    VRFY((sid >= 0), "");

    start[0] = DIMS0 * mpi_rank;
    start[1] = 0;
    block[0] = DIMS0;
    block[1] = DIMS1;
    count[0] = 1;
    count[1] = 1;
    stride[0] = 1;
    stride[1] = 1;

    /* set the selection for this dataset that this process will read from */
    ret = H5Sselect_hyperslab(sid, H5S_SELECT_SET, start, stride, count, block);
    VRFY((ret == 0), "H5Sset_hyperslab succeeded");

    npoints = DIMS0 * DIMS1;
    rbuf = (int *)HDmalloc(npoints * mpi_size * sizeof(int));

    mem_space_id = H5Screate_simple(1, &npoints, NULL);
    VRFY((mem_space_id >= 0), "");

    /* Create dataset transfer property list */
    dxpl_id = H5Pcreate(H5P_DATASET_XFER);
    VRFY((dxpl_id > 0), "H5Pcreate succeeded");

    ret = H5Pset_dxpl_mpio(dxpl_id, H5FD_MPIO_COLLECTIVE);
    VRFY((ret >= 0), "H5Pset_dxpl_mpio succeeded");

    ret = H5Dread(did, H5T_NATIVE_INT, mem_space_id, sid, dxpl_id, rbuf);
    if(ret < 0) FAIL_STACK_ERROR;
    VRFY((ret == 0), "");

    {
        H5D_mpio_actual_io_mode_t io_mode;
        uint32_t local_cause, global_cause;

        ret = H5Pget_mpio_actual_io_mode(dxpl_id, &io_mode);
        VRFY((ret == 0), "");
        VRFY((io_mode == H5D_MPIO_CONTIGUOUS_COLLECTIVE), "Collective Mode invoked");

        H5Pget_mpio_no_collective_cause(dxpl_id, &local_cause, &global_cause);
        VRFY((ret == 0), "");
        VRFY((local_cause == H5D_MPIO_COLLECTIVE), "Collective I/O invoked");
        VRFY((global_cause == H5D_MPIO_COLLECTIVE), "Collective I/O invoked");
    }

    ret = H5Pclose(dxpl_id);
    VRFY((ret == 0), "");

    for(i=0 ; i<npoints ; i++)
        VRFY((rbuf[i] == (mpi_rank+1) * 10), "Data read verified");

    /* check if a process tries to access a different sub-file */
    start[0] = DIMS0 * ((mpi_rank+1) % mpi_size);
    start[1] = 0;
    block[0] = DIMS0;
    block[1] = DIMS1;
    count[0] = 1;
    count[1] = 1;
    stride[0] = 1;
    stride[1] = 1;
    /* set the selection for this dataset that this process will read from */
    ret = H5Sselect_hyperslab(sid, H5S_SELECT_SET, start, stride, count, block);
    VRFY((ret == 0), "H5Sset_hyperslab succeeded");

    H5E_BEGIN_TRY {
        ret = H5Dread(did, H5T_NATIVE_INT, mem_space_id, sid, dxpl_id, rbuf);
    } H5E_END_TRY;
    VRFY((ret < 0), "H5Dwrite failed");

    ret = H5Dclose(did);
    VRFY((ret == 0), "");

    ret = H5Sclose(sid);
    VRFY((ret == 0), "");

    ret = H5Fclose(fid);
    VRFY((ret == 0), "");


    /* try to read with no subfiling on the opened file */

    /* create file access property list */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    VRFY((fapl_id >= 0), "");
    ret = H5Pset_fapl_mpio(fapl_id, comm, info);
    VRFY((ret == 0), "");
    fid = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id);
    VRFY((fid >= 0), "H5Fopen succeeded");
    ret = H5Pclose(fapl_id);
    VRFY((ret == 0), "");

    /* open the dataset */
    did = H5Dopen2(fid, DATASET1, H5P_DEFAULT);
    VRFY((did >= 0), "");

    /* Create dataset transfer property list */
    dxpl_id = H5Pcreate(H5P_DATASET_XFER);
    VRFY((dxpl_id > 0), "H5Pcreate succeeded");

    /* set collective I/O which should be broken */
    ret = H5Pset_dxpl_mpio(dxpl_id, H5FD_MPIO_COLLECTIVE);
    VRFY((ret >= 0), "H5Pset_dxpl_mpio succeeded");

    {
        hsize_t temp = npoints * mpi_size;

        mem_space_id = H5Screate_simple(1, &temp, NULL);
        VRFY((mem_space_id >= 0), "");
    }

    ret = H5Dread(did, H5T_NATIVE_INT, mem_space_id, H5S_ALL, dxpl_id, rbuf);
    if(ret < 0) FAIL_STACK_ERROR;
    VRFY((ret == 0), "");

    {
        uint32_t local_cause, global_cause;

        H5Pget_mpio_no_collective_cause(dxpl_id, &local_cause, &global_cause);
        VRFY((ret == 0), "");
        VRFY((local_cause == H5D_MPIO_VDS_PARALLEL_READ), "Broke Collective I/O");
        VRFY((global_cause == H5D_MPIO_VDS_PARALLEL_READ), "Broke Collective I/O");
    }

    ret = H5Pclose(dxpl_id);
    VRFY((ret == 0), "");

    for(i=0 ; i<npoints*mpi_size ; i++) {
        int temp = i/npoints;

        VRFY((rbuf[i] == (temp%mpi_size + 1) * 10), "Data read verified");
    }

    ret = H5Dclose(did);
    VRFY((ret == 0), "");

    ret = H5Fclose(fid);
    VRFY((ret == 0), "");

    ret = H5Sclose(mem_space_id);
    VRFY((ret == 0), "");

    HDfree(rbuf);

    mrc = MPI_Comm_free(&comm);
    VRFY((mrc==MPI_SUCCESS), "MPI_Comm_free");

    MPI_File_delete(subfile_name, MPI_INFO_NULL);

    return;
error:
    VRFY((1 == 0), "");
} /* subf_2_r */

int main(int argc, char **argv)
{
    int mpi_size, mpi_rank;				/* mpi variables */

#ifndef H5_HAVE_WIN32_API
    /* Un-buffer the stdout and stderr */
    HDsetbuf(stderr, NULL);
    HDsetbuf(stdout, NULL);
#endif

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    if (MAINPROCESS){
	printf("===================================\n");
	printf("Sub-filing Tests Start\n");
	printf("===================================\n");
    }

    /* Attempt to turn off atexit post processing so that in case errors
     * happen during the test and the process is aborted, it will not get
     * hang in the atexit post processing in which it may try to make MPI
     * calls.  By then, MPI calls may not work.
     */
    if (H5dont_atexit() < 0){
	printf("%d: Failed to turn off atexit processing. Continue.\n", mpi_rank);
    };
    H5open();
    h5_show_hostname();

    /* Initialize testing framework */
    TestInit(argv[0], NULL, NULL);

    /* compose the test filenames */
    {
	int i, n;

	n = sizeof(FILENAME)/sizeof(FILENAME[0]) - 1;	/* exclude the NULL */

	for (i=0; i < n; i++)
	    if (h5_fixname(FILENAME[i],fapl,filenames[i],sizeof(filenames[i]))
		== NULL){
		printf("h5_fixname failed\n");
		nerrors++;
		return(1);
	    }
	printf("Test filenames are:\n");
	for (i=0; i < n; i++)
	    printf("    %s\n", filenames[i]);
    }

    AddTest("subf_api", subf_api, NULL, "test subfiling API", PARATESTFILE);
    AddTest("subf_fpp_w", subf_fpp_w, NULL, "test subfiling I/O (file per proc) - Write", PARATESTFILE);
    AddTest("subf_fpp_r", subf_fpp_r, NULL, "test subfiling I/O (file per proc)- Read", PARATESTFILE);
    if(mpi_size > 1) {
        AddTest("subf_2_w", subf_2_w, NULL, "test subfiling I/O to 2 files - Write", PARATESTFILE);
        AddTest("subf_2_r", subf_2_r, NULL, "test subfiling I/O to 2 files - Read", PARATESTFILE);
    }

    /* Display testing information */
    TestInfo(argv[0]);

    /* setup file access property list */
    fapl = H5Pcreate (H5P_FILE_ACCESS);
    H5Pset_fapl_mpio(fapl, MPI_COMM_WORLD, MPI_INFO_NULL);

    /* Parse command line arguments */
    TestParseCmdLine(argc, argv);

    /* Perform requested testing */
    PerformTests();

    /* make sure all processes are finished before final report, cleanup
     * and exit.
     */
    MPI_Barrier(MPI_COMM_WORLD);

    /* Display test summary, if requested */
    if (MAINPROCESS && GetTestSummary())
        TestSummary();


    /* Clean up test files */
    h5_clean_files(FILENAME, fapl);

    nerrors += GetTestNumErrs();

    /* Gather errors from all processes */
    {
        int temp;
        MPI_Allreduce(&nerrors, &temp, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
	nerrors=temp;
    }

    MPI_Finalize();
    return(nerrors!=0);
}