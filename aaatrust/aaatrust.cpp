#include <utility>
#include <string>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>

using eosio::asset;
using eosio::permission_level;
using eosio::action;
using eosio::print;
using eosio::name;

/**
 *  这个合约可以做为商品交易的支付系统。
 *
 *  基本使用流程为：
 *  买家打钱到部署本合约的帐号（买家通过调用prepay来实现）；
 *  收到卖家的东西后，再确认打钱给卖家（买家通过调用confirm来实现）。
 *
 *  注意：
 *  需要为部署本合约的帐号（假设为aaacontractuser）设置 eosio.code 权限，如：
 *  {"permission":{"actor":"aaacontractuser","permission":"eosio.code"}
 *  同时需要为所有买家账号设置 aaacontractuser 帐号的 eosio.code 权限，如：
 *  {"permission":{"actor":"aaacontractuser","permission":"eosio.code"}
 */
class aaatrust : public eosio::contract {
public:
    aaatrust(account_name self) :
            eosio::contract(self),
            records(_self, _self) {}

    /**
     *
     * @param id 唯一的id，用于标识这个买卖过程。上层应用提供它且需要记着它，调用confirm时还需要它
     * @param from 买家
     * @param to 卖家
     * @param price 端口价格
     */
    //@abi action
    void prepay(uint64_t id, const account_name from, const account_name to, const asset &price) {

        eosio_assert(price.is_valid(), "invalid price");
        eosio_assert(price.amount > 0, "price must be positive");

        records.emplace(_self, [&](auto &record) {
            record.id = id;
            record.from = from;
            record.to = to;
            record.price = price;
            record.create_time = current_time();
            record.payed = false;
        });

        action(
                permission_level{from, N(active)},
                N(eosio.token), N(transfer),
                std::make_tuple(from, _self, price, "prepay_for_id:" + std::to_string(id))
        ).send();
    }

    /**
     *
     * @param id 标记买卖的id，对于同一个买卖，应和调用prepay时传的id一致。
     */
    //@abi action
    void confirm(uint64_t id) {
        auto itr = records.find(id);
        eosio_assert(itr != records.end(), "unknown id");

        require_auth(itr->from);
        eosio_assert(!itr->payed, std::string("already payed for id: " + std::to_string(itr->id)).data());

        records.modify(itr, 0, [&](auto &record) {
            record.payed = true;
        });

        action(
                permission_level{_self, N(active)},
                N(eosio.token), N(transfer),
                std::make_tuple(_self, itr->to, itr->price, "confirm_for_id:" + std::to_string(itr->id))
        ).send();
    }

    void clearpayed() {
        require_auth(_self);
        for (auto itr = records.begin(); itr != records.end();) {
            if (itr->payed) {
                itr = records.erase(itr);
            } else {
                itr++;
            }
        }
    }

    //// only for debug
    // void clear(uint64_t id) {
    //     require_auth(_self);
    //     auto itr = records.find(id);
    //     eosio_assert(itr != records.end(), "unknown id");
    //     records.erase(itr);
    // }

private:

    //@abi table records i64
    struct item {
        uint64_t id;
        account_name from;
        account_name to;
        asset price;
        uint64_t create_time;
        bool payed;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(item, (id)(from)(to)(price)(create_time)(payed))
    };

    eosio::multi_index<N(records), item> records;
};

// EOSIO_ABI(aaatrust, (prepay)(confirm)(clearpayed)(clear))
EOSIO_ABI(aaatrust, (prepay)(confirm)(clearpayed))
