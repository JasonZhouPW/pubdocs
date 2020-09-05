OntCversion = '2.0.0'
"""
Smart contract for wrap locking cross chain asset between Ontology and other chains
"""
from ontology.interop.Ontology.Native import Invoke
from ontology.interop.Ontology.Contract import Migrate
from ontology.interop.System.Action import RegisterAction
from ontology.interop.Ontology.Runtime import Base58ToAddress
from ontology.interop.System.Storage import Put, GetContext, Get
from ontology.interop.System.ExecutionEngine import GetExecutingScriptHash
from ontology.interop.System.Runtime import CheckWitness, Notify, Serialize, Deserialize
from ontology.builtins import concat, state, append
from ontology.libont import bytearray_reverse
from ontology.interop.System.App import DynamicAppCall
from ontology.libont import AddressFromVmCode

LOCKER_CONTRACT_ADDRESS = ""
FEE_COLLECTOR_ADDRESS = ""
ADMIN_ADDRESS = ""

WrapLockEvent = RegisterAction("lock", "toChainId", "toAssetHash", "fromAddress","fromAssetHash", "toAddress", "amount","feeAmount")

def Main(operation,args):
    if operation == "lock":
        assert (len(args) == 6)
        fromAssetHash = args[0]
        fromAddress = args[1]
        toChainId = args[2]
        toAddress = args[3]
        amount = args[4]
        feeAmount = args[5]
        return lock(fromAssetHash, fromAddress, toChainId, toAddress, amount,feeAmount)  

    if operation == "upgrade":
        assert (len(args) == 7)
        code = args[0]
        needStorage = args[1]
        name = args[2]
        version = args[3]
        author = args[4]
        email = args[5]
        description = args[6]
        return upgrade(code, needStorage, name, version, author, email, description)

    if operation == "setLockerContract":
        assert(len(args)==1)
        return setLockerContract(args[0])

    if operation == "setFeeCollector":
        assert(len(args)==1)
        return setFeeCollector(args[0])  

    return True

def lock(fromAssetHash, fromAddress, toChainId, toAddress, amount,feeAmount):
    assert(amount >= 0)
    assert(feeAmount >= 0)
    assert(CheckWitness(fromAddress))
    assert(len(toAddress) > 0)

    #call lock contract lock
    res = Invoke(0, LOCKER_CONTRACT_ADDRESS, 'lock', state(fromAssetHash, fromAddress, toChainId, toAddress, amount))
    assert(res == True)

    toAssethash = Invoke(0, LOCKER_CONTRACT_ADDRESS, 'getAssetHash', state(toChainId))
    # transfer fee to fee collector
    res = Invoke(0,fromAssetHash,'transfer',state(fromAddress,FEE_COLLECTOR_ADDRESS,feeAmount))
    assert(res == True)
    WrapLockEvent(toChainId,toAssethash,fromAddress,fromAssetHash,toAddress,amount,feeAmount)
    return True

def upgrade(code, needStorage, name, version, author, email, description):
    """
    upgrade current smart contract to new smart contract.
    :param code:new smart contract avm code.
    :return: True or raise exception.
    """
    owner = getOwner()
    assert (CheckWitness(owner))


    # upgrade smart contract
    res = Migrate(code, needStorage, name, version, author, email, description)
    if not res:
        assert (False)

    Notify(["upgrade smart contract"])

    return True

def getOwner():
    return ADMIN_ADDRESS

def setLockerContract(lockeraddr):
    owner = getOwner()
    assert (CheckWitness(owner))
    LOCKER_CONTRACT_ADDRESS = lockeraddr

def setFeeCollector(feecollector):
    owner = getOwner()
    assert (CheckWitness(owner))
    FEE_COLLECTOR_ADDRESS = feecollector
