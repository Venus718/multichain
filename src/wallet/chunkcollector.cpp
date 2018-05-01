// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"
#include "wallet/chunkcollector.h"

void mc_ChunkEntityKey::Zero()
{
    memset(this,0, sizeof(mc_ChunkEntityKey));    
}

void mc_ChunkEntityValue::Zero()
{
    memset(this,0, sizeof(mc_ChunkEntityValue));    
}


void mc_ChunkCollectorRow::Zero()
{
    memset(this,0, sizeof(mc_ChunkCollectorRow));
}

void mc_ChunkCollectorDBRow::Zero()
{
    memset(this,0, sizeof(mc_ChunkCollectorDBRow));    
}
    uint32_t m_QueryNextAttempt;
    int m_Vout;
    unsigned char m_TxID[MC_TDB_TXID_SIZE];                               
    mc_TxEntity m_Entity;
    unsigned char m_Hash[MC_CDB_CHUNK_HASH_SIZE];                               // Chunk hash
    
    uint32_t m_Size;
    uint32_t m_Flags;
    uint32_t m_QueryAttempts;
    uint32_t m_Status;
    int64_t m_Reserved1;
    int64_t m_Reserved2;        

void mc_ChunkCollector::Zero()
{
    m_DB=NULL;
    m_ChunkDB=NULL;
    m_KeyOffset=0;
    m_KeyDBOffset=0;
    m_KeySize=MC_TDB_TXID_SIZE+sizeof(int)+sizeof(mc_ChunkEntityKey)+3*sizeof(uint32_t);
    m_KeyDBSize=MC_TDB_TXID_SIZE+sizeof(int)+sizeof(uint32_t)+MC_CDB_CHUNK_HASH_SIZE+sizeof(mc_TxEntity);   // 96
    m_ValueOffset=m_KeySize;
    m_ValueDBOffset=m_KeyDBSize;
    m_ValueSize=sizeof(mc_ChunkEntityValue);
    m_ValueDBSize=4*sizeof(uint32_t)+2*sizeof(int64_t);
    m_TotalSize=m_KeySize+m_ValueSize;
    m_TotalDBSize=m_KeyDBSize+m_ValueDBSize;
    m_Name[0]=0;
    m_DBName[0]=0;
    m_NextAutoCommitTimestamp=0;
    m_NextTryTimestamp=0;
    m_MarkPool=NULL;
    m_MemPool=NULL;
    m_MemPoolNext=NULL;
    m_MemPool1=NULL;
    m_MemPool2=NULL;
    
    m_MaxMemPoolSize=MC_CCW_DEFAULT_MEMPOOL_SIZE;
    m_AutoCommitDelay=MC_CCW_DEFAULT_AUTOCOMMIT_DELAY;
    
    m_Semaphore=NULL;
    m_LockedBy=0;     
    
    m_InitMode=0;    
}

int mc_ChunkCollector::Destroy()
{
    if(m_DB)
    {
        m_DB->Close();
        delete m_DB;    
        m_DB=NULL;
    }

    if(m_MarkPool)
    {
        delete m_MarkPool;
    }
    
    if(m_MemPool1)
    {
        delete m_MemPool1;
    }
    
    if(m_MemPool2)
    {
        delete m_MemPool2;
    }

    if(m_Semaphore)
    {
        __US_SemDestroy(m_Semaphore);
    }
     
    
    Zero();
    return MC_ERR_NOERROR;       
}

int mc_ChunkCollector::Lock(int write_mode,int allow_secondary)
{        
    uint64_t this_thread;
    this_thread=__US_ThreadID();
    
    if(this_thread == m_LockedBy)
    {
        return allow_secondary;
    }
    
    __US_SemWait(m_Semaphore); 
    m_LockedBy=this_thread;
    
    return 0;
}

void mc_ChunkCollector::UnLock()
{    
    m_LockedBy=0;
    __US_SemPost(m_Semaphore);
}

int mc_ChunkCollector::Lock()
{        
    return Lock(1,0);
}

void mc_ChunkCollector::SetDBRow(mc_ChunkCollectorRow* collect_row)
{
    m_DBRow.Zero();
    m_DBRow.m_QueryNextAttempt=mc_SwapBytes32(collect_row->m_DBNextAttempt);
    m_DBRow.m_Vout=collect_row->m_Vout;
    memcpy(m_DBRow.m_TxID,collect_row->m_TxID,MC_TDB_TXID_SIZE);
    memcpy(&(m_DBRow.m_Entity),&(collect_row->m_ChunkDef.m_Entity),sizeof(mc_TxEntity));
    memcpy(m_DBRow.m_Hash,collect_row->m_ChunkDef.m_Hash,MC_CDB_CHUNK_HASH_SIZE);
    m_DBRow.m_Size=collect_row->m_ChunkDef.m_Size;
    m_DBRow.m_Flags=collect_row->m_ChunkDef.m_Flags;
    m_DBRow.m_QueryAttempts=collect_row->m_State.m_QueryAttempts;
    m_DBRow.m_Status=collect_row->m_State.m_Status;
}

void mc_ChunkCollector::GetDBRow(mc_ChunkCollectorRow* collect_row)
{
    collect_row->Zero();
    collect_row->m_DBNextAttempt=mc_SwapBytes32(m_DBRow.m_QueryNextAttempt);
    collect_row->m_State.m_QueryNextAttempt=collect_row->m_DBNextAttempt;    
    collect_row->m_Vout=m_DBRow.m_Vout;
    memcpy(collect_row->m_TxID,m_DBRow.m_TxID,MC_TDB_TXID_SIZE);
    memcpy(&(collect_row->m_ChunkDef.m_Entity),&(m_DBRow.m_Entity),sizeof(mc_TxEntity));
    memcpy(collect_row->m_ChunkDef.m_Hash,m_DBRow.m_Hash,MC_CDB_CHUNK_HASH_SIZE);
    collect_row->m_ChunkDef.m_Size=m_DBRow.m_Size;
    collect_row->m_ChunkDef.m_Flags=m_DBRow.m_Flags;
    collect_row->m_State.m_QueryAttempts=m_DBRow.m_QueryAttempts;
    collect_row->m_State.m_Status=m_DBRow.m_Status;
    collect_row->m_State.m_Status |= MC_CCF_INSERTED;                
}

int mc_ChunkCollector::DeleteDBRow(mc_ChunkCollectorRow *collect_row)
{       
    SetDBRow(collect_row);
    return m_DB->Delete((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);    
}

int mc_ChunkCollector::InsertDBRow(mc_ChunkCollectorRow *collect_row)
{
    collect_row->m_State.m_Status &= MC_CCF_ERROR_MASK;
    SetDBRow(collect_row);
    collect_row->m_State.m_Status |= MC_CCF_INSERTED;    
    return m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
}

int mc_ChunkCollector::UpdateDBRow(mc_ChunkCollectorRow *collect_row)
{
    int err;    
    collect_row->m_State.m_Status &= MC_CCF_ERROR_MASK;
    SetDBRow(collect_row);
    err=m_DB->Delete((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);    
    if(err)
    {
        return err;
    }
    collect_row->m_DBNextAttempt=collect_row->m_State.m_QueryNextAttempt;
    m_DBRow.m_QueryNextAttempt=mc_SwapBytes32(collect_row->m_DBNextAttempt);
    collect_row->m_State.m_Status |= MC_CCF_INSERTED;    
    return m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
}

int mc_ChunkCollector::SeekDB(void *dbrow)
{
    int err,value_len;   
    unsigned char *ptr;
    
    err=MC_ERR_NOERROR;
    ptr=(unsigned char*)m_DB->Read((char*)dbrow+m_KeyDBOffset,m_KeyDBSize,&value_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    if(ptr==NULL)
    {
        return MC_ERR_NOT_FOUND;
    }
    
    memcpy((unsigned char*)&m_DBRow+m_KeyDBOffset,(unsigned char*)dbrow,m_KeyDBSize);
    memcpy((unsigned char*)&m_DBRow+m_ValueDBOffset,ptr,m_ValueDBSize);
    memcpy(&m_LastDBRow,&m_DBRow,m_TotalDBSize);
    
    return err;
}

int mc_ChunkCollector::ReadFromDB(mc_Buffer *mempool,int rows)
{
    int err,mprow;   
    mc_ChunkCollectorRow collect_row;
    mc_ChunkDBRow chunk_def;
    unsigned char *ptr;
    int row;
    if(rows <= 0)    
    {
        return MC_ERR_NOERROR;
    }
    
    err=MC_ERR_NOERROR;
    
    ptr=NULL;
    row=0;
    while(row<rows)
    {
        ptr=(unsigned char*)m_DB->MoveNext(&err);
        if(err)
        {
            return MC_ERR_CORRUPTED;            
        }
        if(ptr)
        {
            memcpy((char*)&m_DBRow,ptr,m_TotalDBSize);   
            GetDBRow(&collect_row);
            collect_row.m_State.m_Status |= MC_CCF_INSERTED;                
            if(m_ChunkDB->GetChunkDef(&chunk_def,collect_row.m_ChunkDef.m_Hash,&(collect_row.m_ChunkDef.m_Entity),collect_row.m_TxID,collect_row.m_Vout) == MC_ERR_NOERROR)
            {
                collect_row.m_State.m_Status |= MC_CCF_DELETED;
            }
            mprow=mempool->Seek(&collect_row);
            if(mprow < 0)
            {
                mempool->Add(&collect_row);
                row++;
            }
        }
        else
        {
            row=rows;
        }
    }
    
    memcpy(&m_LastDBRow,&m_DBRow,m_TotalDBSize);
    
    return MC_ERR_NOERROR;
}


void mc_ChunkCollector::Dump(const char *message)
{   
    int i;
    
    if((m_InitMode & MC_WMD_DEBUG) == 0)
    {
        return;
    }
    unsigned char *ptr;
    int dbvalue_len,err;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    FILE *fHan;
    
    sprintf(ShortName,"chunks/collect");
    mc_GetFullFileName(m_Name,ShortName,".dmp",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);
    
    fHan=fopen(FileName,"a");
    if(fHan == NULL)
    {
        return;
    }

    mc_LogString(fHan,message);     
    
    if(m_DB)
    {
        fprintf(fHan,"\nDB\n");
        m_DBRow.Zero();    
        ptr=(unsigned char*)m_DB->Read((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,&dbvalue_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
        if(err)
        {
            return;
        }

        if(ptr)
        {
            memcpy((char*)&m_DBRow+m_ValueDBOffset,ptr,m_ValueDBSize);
            while(ptr)
            {
                mc_MemoryDumpCharSizeToFile(fHan,(char*)&m_DBRow+m_KeyDBOffset,0,m_TotalDBSize,64);        
                ptr=(unsigned char*)m_DB->MoveNext(&err);
                if(ptr)
                {
                    memcpy((char*)&m_DBRow+m_KeyDBOffset,ptr,m_TotalDBSize);            
                }
            }
        }
    }

    fprintf(fHan,"\nMempool\n");
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        mc_MemoryDumpCharSizeToFile(fHan,m_MemPool->GetRow(i),0,m_TotalSize,64);    
    }
    
    fprintf(fHan,"\n<<<<<< \tChain height: %6d\t%s\n\n",mc_gState->m_Permissions->m_Block,message);
    fclose(fHan);
}

int mc_ChunkCollector::Initialize(mc_ChunkDB *chunk_db,const char *name,uint32_t mode)
{
    int err=MC_ERR_NOERROR;    
    if(name)
    {
        strcpy(m_Name,name);

        m_DB=new mc_Database;

        mc_GetFullFileName(name,"chunks/collect",".db",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_DBName);

        m_DB->SetOption("KeySize",0,m_KeyDBSize);
        m_DB->SetOption("ValueSize",0,m_ValueDBSize);


        err=m_DB->Open(m_DBName,MC_OPT_DB_DATABASE_CREATE_IF_MISSING | MC_OPT_DB_DATABASE_TRANSACTIONAL | MC_OPT_DB_DATABASE_LEVELDB);

        if(err)
        {
            return err;
        }
    }

    m_InitMode=mode;
    m_ChunkDB=chunk_db;
    
    
    m_MarkPool=new mc_Buffer;                                                
    err=m_MarkPool->Initialize(m_KeySize,m_TotalSize,MC_BUF_MODE_DEFAULT);
    m_MemPool1=new mc_Buffer;                                                
    err=m_MemPool1->Initialize(m_KeySize,m_TotalSize,MC_BUF_MODE_MAP);
    m_MemPool2=new mc_Buffer;                                                
    err=m_MemPool2->Initialize(m_KeySize,m_TotalSize,MC_BUF_MODE_MAP);
    
    m_MemPool=m_MemPool1;

    m_DBRow.Zero();
    
    if(m_DB)
    {
        err=SeekDB(&m_DBRow);
        if(err)
        {
            if(err != MC_ERR_NOT_FOUND)
            {
                return err;
            }
        }
        if(err != MC_ERR_NOT_FOUND)
        {
            err=ReadFromDB(m_MemPool,m_MaxMemPoolSize);
            if(err)
            {
                return err;
            }
        }
        else
        {
            err=m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            if(err)
            {
                return err;     
            }              
            err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
            if(err)
            {
                return err;
            }                            
        }
    }
/*        
        ptr=(unsigned char*)m_DB->Read((char*)&collect_row+m_KeyOffset,m_KeySize,&value_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
        if(err)
        {
            return err;
        }

        if(ptr)                                                                     
        {   
            ptr=(unsigned char*)m_DB->MoveNext(&err);
            collect_row.Zero();
            if(ptr)
            {
                memcpy((char*)&collect_row,ptr,m_TotalDBSize);
            }
            while(ptr)
            {
                ptr=(unsigned char*)m_DB->MoveNext(&err);
                if(err)
                {
                    return MC_ERR_CORRUPTED;            
                }
                collect_row.m_State.m_Status |= MC_CCF_INSERTED;                
                if(m_ChunkDB->GetChunkDef(&chunk_def,collect_row.m_ChunkDef.m_Hash,&(collect_row.m_ChunkDef.m_Entity),collect_row.m_TxID,collect_row.m_Vout) == MC_ERR_NOERROR)
                {
                    collect_row.m_State.m_Status |= MC_CCF_DELETED;
                }
                m_MemPool->Add(&collect_row);
                collect_row.Zero();
                if(ptr)
                {
                    memcpy((char*)&collect_row,ptr,m_TotalDBSize);
                }
            }        
        }
        else
        {
            err=m_DB->Write((char*)&collect_row+m_KeyOffset,m_KeySize,(char*)&collect_row+m_ValueOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            if(err)
            {
                return err;     
            }              
            err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
            if(err)
            {
                return err;
            }                
        }
    }
 */ 
    
    Dump("Initialize");
    
    return err;   
}

int mc_ChunkCollector::InsertChunk(                                                            // Adds chunk to mempool
                 const unsigned char *hash,                                     // Chunk hash (before chopping)    
                 const mc_TxEntity *entity,                                     // Parent entity
                 const unsigned char *txid,
                 const int vout,
                 const uint32_t chunk_size)
{
    int err;
    
    Lock();
    err=InsertChunkInternal(hash,entity,txid,vout,chunk_size);
    UnLock();
    
    return err;    
}
    
int mc_ChunkCollector::InsertChunkInternal(                  
                 const unsigned char *hash,   
                 const mc_TxEntity *entity,   
                 const unsigned char *txid,
                 const int vout,
                 const uint32_t chunk_size)
{
    mc_ChunkCollectorRow collect_row;
    int mprow;
    
    collect_row.Zero();
    memcpy(collect_row.m_ChunkDef.m_Hash,hash,MC_CDB_CHUNK_HASH_SIZE);
    memcpy(&collect_row.m_ChunkDef.m_Entity,entity,sizeof(mc_TxEntity));
    memcpy(collect_row.m_TxID,txid,MC_TDB_TXID_SIZE);
    collect_row.m_Vout=vout;
    collect_row.m_ChunkDef.m_Size=chunk_size;
    collect_row.m_State.m_Status=MC_CCF_NEW;
    
    mprow=m_MemPool->Seek(&collect_row);
    if(mprow<0)
    {
        m_MemPool->Add(&collect_row);
    }
    
    return MC_ERR_NOERROR;
}
    
int mc_ChunkCollector::MarkAndClear(uint32_t flag, int unmark)
{
    int i,mprow;
    
    mc_ChunkCollectorRow *row;
    Lock();    
    
    for(i=0;i<m_MarkPool->GetCount();i++)
    {
        mprow=m_MemPool->Seek(m_MarkPool->GetRow(i));
        if(mprow >= 0)
        {
            row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(mprow);
            if(unmark)
            {
                row->m_State.m_Status &= ~flag;
            }
            else
            {
                row->m_State.m_Status |= flag;
            }
        }
    }
    
    m_MarkPool->Clear();
    UnLock();    
    
    return MC_ERR_NOERROR;
}

int mc_ChunkCollector::CopyFlags()
{
    int i,mprow;
    
    mc_ChunkCollectorRow *row;
    mc_ChunkCollectorRow *mark_row;
    Lock();    
    
    for(i=0;i<m_MarkPool->GetCount();i++)
    {
        mark_row=(mc_ChunkCollectorRow *)m_MarkPool->GetRow(i);
        mprow=m_MemPool->Seek(mark_row);
        if(mprow >= 0)
        {
            row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(mprow);
            row->m_State.m_Status=mark_row->m_State.m_Status;
        }
    }
    
    m_MarkPool->Clear();
    UnLock();    
    
    return MC_ERR_NOERROR;
}

int mc_ChunkCollector::FillMarkPoolByHash(const unsigned char *hash)
{
    int i;    
    mc_ChunkCollectorRow *row;
    
    Lock();
    
    m_MarkPool->Clear();
    
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(i);        
        if(memcmp(row->m_ChunkDef.m_Hash,hash,MC_CDB_CHUNK_HASH_SIZE) == 0)
        {
            m_MarkPool->Add(row);
        }
    }
    
    UnLock();        
    
    return MC_ERR_NOERROR;
}

int mc_ChunkCollector::FillMarkPoolByFlag(uint32_t flag, uint32_t not_flag)
{
    int i;    
    mc_ChunkCollectorRow *row;
    
    Lock();
    
    m_MarkPool->Clear();
    
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(i);        
        if(row->m_State.m_Status & flag)
        {
            if( (row->m_State.m_Status & not_flag) == 0)
            {
                m_MarkPool->Add(row);
            }
        }
    }
    
    UnLock();        
    
    return MC_ERR_NOERROR;
}

int mc_ChunkCollector::Commit()
{
    int err;
    
    Lock();
    err=CommitInternal();
    UnLock();
    
    return err;        
}

int mc_ChunkCollector::CommitInternal()
{
    int i;    
    mc_ChunkCollectorRow *row;
    int err,commit_required;
    uint32_t time_now;

    err=MC_ERR_NOERROR;
    
    if(m_DB == NULL)
    {
        return MC_ERR_NOT_ALLOWED;
    }
    
    Dump("Before Commit");
    
    time_now=mc_TimeNowAsUInt();
    
    if(m_MemPool == m_MemPool1)
    {
        m_MemPoolNext=m_MemPool2;
    }
    else
    {
        m_MemPoolNext=m_MemPool1;        
    }
    
    m_MemPoolNext->Clear();
    
    commit_required=0;
    
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(i);        
        
        if(row->m_State.m_Status & MC_CCF_DELETED)
        {
            if(row->m_State.m_Status & MC_CCF_INSERTED)
            {
                commit_required=1;
                DeleteDBRow(row);
//                m_DB->Delete((char*)row+m_KeyOffset,m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            }            
        }
        else
        {
            if( (row->m_State.m_Status & MC_CCF_INSERTED) == 0 )
            {
                commit_required=1;
                InsertDBRow(row);
            }
            else
            {
                if(row->m_State.m_Status & MC_CCF_UPDATED)
                {
                    commit_required=1;
                    UpdateDBRow(row);                
                }
            }
/*            
            if( ((row->m_State.m_Status & MC_CCF_INSERTED) == 0 ) || (row->m_State.m_Status & MC_CCF_UPDATED) )
            {
                
                row->m_State.m_Status &= MC_CCF_ERROR_MASK;
                m_DB->Write((char*)row+m_KeyOffset,m_KeySize,(char*)row+m_ValueOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                row->m_State.m_Status |= MC_CCF_INSERTED;
            }            
 */ 
            if(!row->m_State.m_Query.IsZero() || (row->m_State.m_QueryNextAttempt <= time_now))
            {
                m_MemPoolNext->Add(row);                
            }
        }
    }    

    if(commit_required)
    {
        err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return err;
        }                
    }
   
    if(m_MemPoolNext->GetCount() < m_MaxMemPoolSize)
    {
        m_LastDBRow.Zero();
        err=SeekDB(&m_LastDBRow);
        if(err == MC_ERR_NOERROR)
        {
            ReadFromDB(m_MemPoolNext,m_MaxMemPoolSize);
        }
    }
    
    m_MemPool->Clear();
    m_MemPool=m_MemPoolNext;
    
    Dump("Commit");
    
    return err;
}

