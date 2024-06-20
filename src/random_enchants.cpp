#include "random_enchants.h"
#include <tuple>
#include <cmath>

void rollPossibleEnchant(Player* player, Item* item)
{
    // Check global enable option
    if (!sConfigMgr->GetOption<bool>("RandomEnchants.Enable", true))
    {
        return;
    }

    uint32 itemQuality = item->GetTemplate()->Quality;
    uint32 itemClass = item->GetTemplate()->Class;

    /* eliminates enchanting anything that isn't a recognized quality */
    /* eliminates enchanting anything but weapons/armor */
    if ((itemQuality > 5 || itemQuality < 1) || (itemClass != 2 && itemClass != 4))
    {
        return;
    }

    int slotRand = -1;
    uint32 slotEnch = 5; // enchanting the "bonus enchant" slot only
    int rarityRoll = -1;
    uint8 tier = 0;
    uint32 enchantId;

    // Fetching the configuration values as float
    float enchantChance1 = sConfigMgr->GetOption<float>("RandomEnchants.EnchantChance1", 70.0f);
    float enchantChance2 = sConfigMgr->GetOption<float>("RandomEnchants.EnchantChance2", 65.0f); // not used
    float enchantChance3 = sConfigMgr->GetOption<float>("RandomEnchants.EnchantChance3", 60.0f); // not used

    // Rolling if enchant applies
    if (rand_chance() < enchantChance1) {
        std::tie(enchantId, rarityRoll, tier) = getRandEnchantment(item);
        slotRand = static_cast<int>(enchantId);
    }

    // Appling enchant
    if (slotRand != -1)
    {
        if (sSpellItemEnchantmentStore.LookupEntry(slotRand))
        {   //Make sure enchantment id exists
            player->ApplyEnchantment(item, EnchantmentSlot(slotEnch), false);
            item->SetEnchantment(EnchantmentSlot(slotEnch), slotRand, 0, 0);
            player->ApplyEnchantment(item, EnchantmentSlot(slotEnch), true);
        }
    }

    ChatHandler chathandle = ChatHandler(player->GetSession());

    uint8 loc_idx = player->GetSession()->GetSessionDbLocaleIndex();
    const ItemTemplate* temp = item->GetTemplate();
    std::string name = temp->Name1;
    if (ItemLocale const* il = sObjectMgr->GetItemLocale(temp->ItemId))
        ObjectMgr::GetLocaleString(il->Name, loc_idx, name);

    if (slotRand != -1)
    {
        chathandle.PSendSysMessage("Your |cffFF0000 %s |rwas found with an extra enchantment! (Rarity Roll: %d - Tier: %u)", name, rarityRoll, tier);
    }
}

std::tuple<uint32, int, uint8> getRandEnchantment(Item* item)
{
    uint32 itemLevel = item->GetTemplate()->ItemLevel;
    uint32 itemClass = item->GetTemplate()->Class;
    uint32 itemQuality = item->GetTemplate()->Quality;
    std::string classQueryString = "";
    int rarityRoll = itemLevel;
    uint8 tier = 0;

    switch (itemClass)
    {
        case 2:
            classQueryString = "WEAPON";
            break;
        case 4:
            classQueryString = "ARMOR";
            break;
    }

    if (classQueryString == "")
        return std::make_tuple(0, 0, 0);

    // Item level flat addition to rarity roll
    if (itemLevel <= 200) // if northrend HC dungeon or less... 
        switch (itemQuality) // give bonus on quality AND...
        {
            case PURPLE:
                rarityRoll = std::ceil(rarityRoll * 1.05);
                break;
            case ORANGE:
                rarityRoll = std::ceil(rarityRoll * 1.1);
                break;
        }
        rarityRoll += (rand_norm() * 5); // ... +random (1-5)
    // if item level 200 or less, then it's just raid gear, so no changes. just its flat item level will scale w enchants

    // check negs, just in case
    if (rarityRoll < 0)
        return std::make_tuple(0, 0, 0);

    // check tiers
    // item level threshold was determined by... 
    // round(min(level range) + (level range)/2,0) 
    if (rarityRoll <= 14) // Lvl Req 1,10: itemLevel 28-1
        tier = 1;
    else if (rarityRoll <= 20) // Lvl Req 10,15: itemLevel 31-10
        tier = 2;
    else if (rarityRoll <= 25) // Lvl Req 15,20: itemLevel 35-16
        tier = 3;
    else if (rarityRoll <= 29) // Lvl Req 20,25: itemLevel 40-19
        tier = 4;
    else if (rarityRoll <= 39) // Lvl Req 25,30: itemLevel 50-28
        tier = 5;
    else if (rarityRoll <= 41) // Lvl Req 30,35: itemLevel 50-32
        tier = 6;
    else if (rarityRoll <= 48) // Lvl Req 35,40: itemLevel 59-36
        tier = 7;
    else if (rarityRoll <= 50) // Lvl Req 40,45: itemLevel 60-40
        tier = 8;
    else if (rarityRoll <= 54) // Lvl Req 45,50: itemLevel 60-48
        tier = 9;
    else if (rarityRoll <= 56) // Lvl Req 50,55: itemLevel 61-52
        tier = 10;
    else if (rarityRoll <= 58) // Lvl Req 55,60(vanilla): itemLevel 65-52
        tier = 11;
    else if (rarityRoll <= 88) // Lvl Req 57(outland),65: itemLevel 111-66
        tier = 12;
    else if (rarityRoll <= 102) // Lvl Req 65,70(outland): itemLevel 120-83
        tier = 13;
    else if (rarityRoll <= 141) // Lvl Req 67(northrend),75: itemLevel 162-120
        tier = 14;
    else if (rarityRoll <= 168) // Lvl Req 75, 80(non dungeon): itemLevel 182-154
        tier = 15;
    else if (rarityRoll <= 200) // Northrend/Northrend HC Dungeons: itemLevel 200-180
        tier = 16;
    else if (rarityRoll <= 231) // Naxx/EoE/OS (Tier 7) + Uldar (Tier 8): itemLevel 239-200
        tier = 17;
    else if (rarityRoll <= 250) // ToC/Ony (Tier 9): itemLevel 245-232
        tier = 18;
    else if (rarityRoll <= 276) // ICC/RS (Tier 10): itemLevel 264-251
        tier = 19;
    else // 25M HC ICC/RS 284-277
        tier = 20;

    QueryResult result = WorldDatabase.Query("SELECT `enchantID` FROM `item_enchantment_random_tiers` WHERE `tier`={} AND `exclusiveSubClass`=NULL AND exclusiveSubClass='{}' OR `class`='{}' OR `class`='ANY' ORDER BY RAND() LIMIT 1", tier, item->GetTemplate()->SubClass, classQueryString, classQueryString);

    if (!result)
        return std::make_tuple(0, 0, 0);

    return std::make_tuple(result->Fetch()[0].Get<uint32>(), rarityRoll, tier);
}

void RandomEnchantsPlayer::OnLogin(Player* player)
{
    if (sConfigMgr->GetOption<bool>("RandomEnchants.AnnounceOnLogin", true) && (sConfigMgr->GetOption<bool>("RandomEnchants.Enable", true)))
        ChatHandler(player->GetSession()).SendSysMessage(sConfigMgr->GetOption<std::string>("RandomEnchants.OnLoginMessage", "This server is running a RandomEnchants Module.").c_str());
}

void RandomEnchantsPlayer::OnLootItem(Player* player, Item* item, uint32 /*count*/, ObjectGuid /*lootguid*/)
{
    if (sConfigMgr->GetOption<bool>("RandomEnchants.OnLoot", true) && sConfigMgr->GetOption<bool>("RandomEnchants.Enable", true))
        rollPossibleEnchant(player, item);
}

void RandomEnchantsPlayer::OnCreateItem(Player* player, Item* item, uint32 /*count*/)
{
    if (sConfigMgr->GetOption<bool>("RandomEnchants.OnCreate", true) && (sConfigMgr->GetOption<bool>("RandomEnchants.Enable", true)))
        rollPossibleEnchant(player, item);
}

void RandomEnchantsPlayer::OnQuestRewardItem(Player* player, Item* item, uint32 /*count*/)
{
    if (sConfigMgr->GetOption<bool>("RandomEnchants.OnQuestReward", true) && (sConfigMgr->GetOption<bool>("RandomEnchants.Enable", true)))
        rollPossibleEnchant(player, item);
}

void RandomEnchantsPlayer::OnGroupRollRewardItem(Player* player, Item* item, uint32 /*count*/, RollVote /*voteType*/, Roll* /*roll*/)
{
    if (sConfigMgr->GetOption<bool>("RandomEnchants.OnGroupRoll", true) && (sConfigMgr->GetOption<bool>("RandomEnchants.Enable", true)))
        rollPossibleEnchant(player, item);
}