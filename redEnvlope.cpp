#include<ontiolib/ontio.hpp>
#include<ontiolib/base58.hpp>
#include <string>

using namespace ontio;

class redEnvlope: public contract{
    std::string rePrefix = "RE_PREFIX_";
    std::string sentPrefix = "SENT_COUNT_";
    std::string claimPrefix = "CLAIM_PREFIX_";
    address ONTAddress = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
    address ONGAddress = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2};


    struct receiveRecord{
        address account;
        asset amount;
        ONTLIB_SERIALIZE(receiveRecord,(account)(amount))
    };

    struct envlopeStruct{
        address tokenAddress;
        asset totalAmount;
        asset totalPackageCount;
        asset remainAmount;
        asset remainPackageCount;
        std::vector<struct receiveRecord> records;
        ONTLIB_SERIALIZE( envlopeStruct,  (tokenAddress)(totalAmount)(totalPackageCount)(remainAmount)(remainPackageCount)(records) )
    };

    public:
    using contract::contract;

    /*
    *   createRedEnvlope
    *   owner    : creator address
    *   packcount: count of the envlope
    *   amount   : total amount of the envlope
    *   tokenAddr: token address
    * */
    bool createRedEnvlope(address owner,asset packcount, asset amount,address tokenAddr ){
        //check the owner rights
        ontio_assert(check_witness(owner),"checkwitness failed");
        if (isONTToken(tokenAddr)){
            ontio_assert(amount >= packcount,"ont amount should greater than packcount");
        }

        key sentkey = make_key(sentPrefix,owner.tohexstring());
        asset sentcount = 0;
        storage_get(sentkey,sentcount);
        sentcount += 1;
        storage_put(sentkey,sentcount);

        H256 hash ;

        hash256(make_key(owner,sentcount),hash) ;

        key rekey = make_key(rePrefix,hash256ToHexstring(hash));
        address selfaddr = self_address();

        //determin the token type
        if (isONTToken(tokenAddr)){

            bool result = ont::transfer(owner,selfaddr ,amount);
            ontio_assert(result,"transfer native token failed!");
           
        }else if (isONGToken(tokenAddr)){

            bool result = ong::transfer(owner,selfaddr ,amount);
            ontio_assert(result,"transfer native token failed!");
        }else{
            std::vector<char> params = pack(std::string("transfer"),owner,selfaddr,amount);
            bool res; 
            call_contract(tokenAddr,params, res );

            ontio_assert(res,"transfer oep4 token failed!");
        }
        struct envlopeStruct es ;
        es.tokenAddress = tokenAddr;
        es.totalAmount = amount;
        es.totalPackageCount = packcount;
        es.remainAmount = amount;
        es.remainPackageCount = packcount;
        es.records = {};
        storage_put(rekey, es);

        char buffer [100];
        sprintf(buffer, "{\"states\":[\"%s\", \"%s\", \"%s\"]}","createEnvlope",owner.tohexstring().c_str(),hash256ToHexstring(hash).c_str());

        notify(buffer);
        return true;

    }

    /*
    * queryEnvlope
    * hash   : envlope hash
    * return : json
    **/

    std::string queryEnvlope(std::string hash){
        key rekey = make_key(rePrefix,hash);
        struct envlopeStruct es;
        storage_get(rekey,es);
        return formatEnvlope(es);
    }

    /*
    * claimEnvlope  
    * account : claimer account
    * hash : envlope hash
    * */
    bool claimEnvlope(address account, std::string hash){
        ontio_assert(check_witness(account),"checkwitness failed");
        key claimkey = make_key(claimPrefix,hash,account);
        asset claimed = 0 ;
        storage_get(claimkey,claimed);
        ontio_assert(claimed == 0,"you have claimed this envlope!");

        key rekey = make_key(rePrefix,hash);
        struct envlopeStruct es;
        storage_get(rekey,es);
        ontio_assert(es.remainAmount > 0, "the envlope has been claimed over!");
        ontio_assert(es.remainPackageCount > 0, "the envlope has been claimed over!");
        // std::vector<struct receiveRecord> records = es.records;
        struct receiveRecord record ;
        record.account = account;
        asset claimAmount = 0;

        if (es.remainPackageCount == 1){
            claimAmount = es.remainAmount;
            record.amount = claimAmount;
        }else{
            H256 random = current_blockhash() ;
            char part[8];
            memcpy(part,&random,8);
            uint64_t random_num = *(uint64_t*)part;
            uint32_t percent = random_num % 100 + 1;

            claimAmount = es.remainAmount * percent / 100;
            //ont case
            if (claimAmount == 0){
                claimAmount = 1;
            }else if(isONTToken(es.tokenAddress)){
                if ( (es.remainAmount - claimAmount) < (es.remainPackageCount - 1)){
                    claimAmount = es.remainAmount - es.remainPackageCount + 1;
                }
            }

            record.amount = claimAmount;

        }
        es.remainAmount -= claimAmount;
        es.remainPackageCount -= 1;
        es.records.push_back(record);

        address selfaddr = self_address();
        if (isONTToken(es.tokenAddress)){
            bool result = ont::transfer(selfaddr,account ,claimAmount);
            ontio_assert(result,"transfer ont token failed!");
        } else  if (isONGToken(es.tokenAddress)){
            bool result = ong::transfer(selfaddr,account ,claimAmount);
            ontio_assert(result,"transfer ong token failed!");
        } else{
            std::vector<char> params = pack(std::string("transfer"),selfaddr,account,claimAmount);

            bool res = false; 
            call_contract(es.tokenAddress,params, res );
            ontio_assert(res,"transfer oep4 token failed!");
        }
        storage_put(claimkey,claimAmount);
        storage_put(rekey,es);
        char buffer [100];        
        std::sprintf(buffer, "{\"states\":[\"%s\",\"%s\",\"%s\",\"%lld\"]}","claimEnvlope",hash.c_str(),account.tohexstring().c_str(),claimAmount);

        notify(buffer);
        return true;
    }
    
    private:
    bool isONGToken(address tokenAddr){
        return  tokenAddr == ONGAddress ;
    }
    bool isONTToken(address tokenAddr){
        return tokenAddr == ONTAddress ;
    }
    std::string hash256ToHexstring(H256 hash){
        std::string s;
        
		s.resize(64);
		int index = 0;
		for(auto it:hash) {
			uint8_t high = it/16, low = it%16;
			s[index] = (high<10) ? ('0' + high) : ('a' + high - 10);
			s[index + 1] = (low<10) ? ('0' + low) : ('a' + low - 10);
			index += 2;
		}
		return s;
    }

    std::string formatEnvlope(struct envlopeStruct es){
        char buf[100];
        std::vector<struct receiveRecord> records = es.records;
        std::sprintf(buf,"{\"tokenAddress\":\"%s\",\"totalAmount\":%lld,\"totalPackageAmount\":%lld,\"remainAmount\":%lld,\"remainPackageAmount\":%lld,\"claimers\":[",es.tokenAddress.tohexstring().c_str(),es.totalAmount,es.totalPackageCount,es.remainAmount,es.remainPackageCount);
        std::string s;
        if ( records.size() > 0){
            int i = 0;
            for (auto r :records){
                s += formatRecord(r);
                i++;
                if (i < records.size()){
                    s += ",";
                }
            }
        }
        s += "]}";
        std::string ss(buf);
        return ss +s;

    }

    std::string formatRecord(struct receiveRecord record){

        char buf[100] ;
        std::sprintf(buf,"{\"account\":\"%s\",\"amount\":%lld}",record.account.tohexstring().c_str(),record.amount);
        std::string s(buf);
        return s;
    }
};

ONTIO_DISPATCH(redEnvlope, (createRedEnvlope)(queryEnvlope)(claimEnvlope));
