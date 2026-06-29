#include "Global.h"
#include "gmlib/mc/world/actor/Player.h"
#include "ll/api/memory/Hook.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/network/packet/AddPlayerPacket.h"
#include "mc/network/packet/PlayerListPacket.h"
#include "mc/network/packet/PlayerListPacketType.h"
#include "mc/network/packet/RemoveActorPacket.h"
#include "mc/network/packet/SetPlayerGameTypePacket.h"
#include "mc/network/packet/UpdateAbilitiesPacket.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/PlayerListEntry.h"

#include <cstdlib>

PlayerListEntry::PlayerListEntry(PlayerListEntry const&) {
    std::terminate();
}

std::unordered_set<uint64> FreeCamList;

namespace FreeCamera {

void EnableFreeCameraPacket(Player* pl) {
    SetPlayerGameTypePacket pkt;
    pkt.mPlayerGameType = GameType::Spectator;
    pl->sendNetworkPacket(pkt);
}

void SendFakePlayerPacket(Player* pl) {
    auto randomUuid = mce::UUID::random();
    auto uniqueId   = pl->getOrCreateUniqueID();
    uniqueId.rawID  = uniqueId.rawID + 114514;

    {
        PlayerListPacket listPkt;
        listPkt.mAction = PlayerListPacketType::Add;
        auto& entries   = listPkt.mEntries.get();
        entries.reserve(1);
        entries.emplace_back(*pl);
        auto& entry = entries.back();
        entry.mUUID = randomUuid;
        entry.mId   = uniqueId;
        pl->sendNetworkPacket(listPkt);
    }

    {
        auto pkt1      = AddPlayerPacket(*pl);
        pkt1.mEntityId = uniqueId;
        pkt1.mUuid     = randomUuid;
        pl->sendNetworkPacket(pkt1);
    }

    {
        PlayerListPacket listPkt;
        listPkt.mAction = PlayerListPacketType::Remove;
        auto& entries   = listPkt.mEntries.get();
        entries.reserve(1);
        entries.emplace_back(*pl);
        auto& entry = entries.back();
        entry.mUUID = randomUuid;
        entry.mId   = uniqueId;
        pl->sendNetworkPacket(listPkt);
    }
}

void DisableFreeCameraPacket(Player* pl) {
    SetPlayerGameTypePacket gameTypePkt;
    gameTypePkt.mPlayerGameType = pl->getPlayerGameType();
    pl->sendNetworkPacket(gameTypePkt);

    auto uniqueId  = pl->getOrCreateUniqueID();
    uniqueId.rawID = uniqueId.rawID + 114514;
    RemoveActorPacket removePkt;
    removePkt.mEntityId = uniqueId;
    pl->sendNetworkPacket(removePkt);

    UpdateAbilitiesPacket abilitiesPkt(pl->getOrCreateUniqueID(), pl->getAbilities());
    pl->sendNetworkPacket(abilitiesPkt);
}

/*
void SendActorLinkPacket(Player* pl) {
    auto links = pl->getLinks();
    for (auto& link : links) {
        GMLIB_BinaryStream bs;
        if (ll::service::getLevel()->getPlayer(link.A)) {
            bs.writeVarInt64(link.A.id + 114514);
        } else {
            bs.writeVarInt64(link.A.id);
        }
        if (ll::service::getLevel()->getPlayer(link.B)) {
            bs.writeVarInt64(link.B.id + 114514);
        } else {
            bs.writeVarInt64(link.B.id);
        }
        bs.writeUnsignedChar((uchar)link.type);
        bs.writeBool(link.mImmediate);
        bs.writeBool(link.mPassengerInitiated);
        GMLIB::Server::NetworkPacket<(int)MinecraftPacketIds::SetActorLink> pkt(bs.getAndReleaseData());
        pl->sendNetworkPacket(pkt);
    }
}
*/

void EnableFreeCamera(Player* pl) {
    FreeCamList.insert(pl->getNetworkIdentifier().mGuid.g);
    EnableFreeCameraPacket(pl);
    SendFakePlayerPacket(pl);
    // SendActorLinkPacket(pl);
}

void DisableFreeCamera(Player* pl) {
    auto pos   = pl->getFeetPos();
    auto dimid = pl->getDimensionId();
    // auto links = pl->getLinks();
    FreeCamList.erase(pl->getNetworkIdentifier().mGuid.g);
    DisableFreeCameraPacket(pl);
    pl->teleport(pos, dimid);
    // for (auto& link : links) {
    //     auto ride  = ll::service::getLevel()->fetchEntity(link.A);
    //     auto rider = ll::service::getLevel()->fetchEntity(link.B);
    //     if (ride && rider) {
    //        rider->startRiding(*ride);
    //    }
    //}
}

LL_TYPE_INSTANCE_HOOK(
    ServerPlayerMoveHandleEvent,
    ll::memory::HookPriority::Normal,
    ServerNetworkHandler,
    &ServerNetworkHandler::$handle,
    void,
    NetworkIdentifier const&     id,
    PlayerAuthInputPacket const& pkt
) {
    if (FreeCamList.contains(id.mGuid.g)) {
        return;
    } else {
        return origin(id, pkt);
    }
}

LL_TYPE_INSTANCE_HOOK(
    PlayerGamemodeChangeEvent,
    ll::memory::HookPriority::Normal,
    ServerPlayer,
    &ServerPlayer::$setPlayerGameType,
    void,
    ::GameType gamemode
) {
    origin(gamemode);
    if (FreeCamList.contains(getNetworkIdentifier().mGuid.g)) {
        DisableFreeCamera(this);
    }
}

LL_TYPE_INSTANCE_HOOK(
    PlayerHurtEvent,
    ll::memory::HookPriority::Normal,
    Mob,
    &Mob::getDamageAfterResistanceEffect,
    float,
    class ActorDamageSource const& a1,
    float                          a2
) {
    auto res = origin(a1, a2);
    if (this->isType(ActorType::Player) && res != 0) {
        auto pl = (Player*)this;
        if ((pl->isSurvival() || pl->isAdventure()) && FreeCamList.contains(pl->getNetworkIdentifier().mGuid.g)) {
            DisableFreeCamera(pl);
        }
    }
    return res;
}

LL_TYPE_INSTANCE_HOOK(
    PlayerDieEvent,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::$die,
    void,
    class ActorDamageSource const& a1
) {
    if (FreeCamList.contains(getNetworkIdentifier().mGuid.g)) {
        DisableFreeCamera(this);
    }
    return origin(a1);
}

LL_TYPE_INSTANCE_HOOK(
    PlayerLeftEvent,
    ll::memory::HookPriority::Normal,
    ServerPlayer,
    &ServerPlayer::disconnect,
    void
) {
    FreeCamList.erase(getNetworkIdentifier().mGuid.g);
    return origin();
}

struct Impl {
    ll::memory::HookRegistrar<
        ServerPlayerMoveHandleEvent,
        PlayerGamemodeChangeEvent,
        PlayerHurtEvent,
        PlayerDieEvent,
        PlayerLeftEvent>
        r;
};

std::unique_ptr<Impl> impl;

void freecameraHook(bool enable) {
    if (enable) {
        if (!impl) impl = std::make_unique<Impl>();
    } else {
        impl.reset();
    }
};

} // namespace FreeCamera