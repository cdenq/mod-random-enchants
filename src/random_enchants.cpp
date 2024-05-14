#include "random_enchants.h"
#include <tuple>

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

    int slotRand[3] = { -1, -1, -1 };
    uint32 slotEnch[3] = { 0, 1, 5 };
    std::array<int, 3> rarityRoll = { -1, -1, -1 };
    std::array<uint8, 3> tier = { 0, 0, 0 };
    uint32 enchantId;

    // Fetching the configuration values as float
    float enchantChance1 = sConfigMgr->GetOption<float>("RandomEnchants.EnchantChance1", 70.0f);
    float enchantChance2 = sConfigMgr->GetOption<float>("RandomEnchants.EnchantChance2", 65.0f);
    float enchantChance3 = sConfigMgr->GetOption<float>("RandomEnchants.EnchantChance3", 60.0f);

    if (rand_chance() < enchantChance1)
    {
        std::tie(enchantId, rarityRoll[0], tier[0]) = getRandEnchantment(item);
        slotRand[0] = static_cast<int>(enchantId);
    }
    if (slotRand[0] != -1 && rand_chance() < enchantChance2)
    {
        std::tie(enchantId, rarityRoll[1], tier[1]) = getRandEnchantment(item);
        slotRand[1] = static_cast<int>(enchantId);
    }
    if (slotRand[1] != -1 && rand_chance() < enchantChance3)
    {
        std::tie(enchantId, rarityRoll[2], tier[2]) = getRandEnchantment(item);
        slotRand[2] = static_cast<int>(enchantId);
    }

    for (int i = 0; i < 3; i++)
    {
        if (slotRand[i] != -1)
        {
            if (sSpellItemEnchantmentStore.LookupEntry(slotRand[i]))
            {   //Make sure enchantment id exists
                player->ApplyEnchantment(item, EnchantmentSlot(slotEnch[i]), false);
                item->SetEnchantment(EnchantmentSlot(slotEnch[i]), slotRand[i], 0, 0);
                player->ApplyEnchantment(item, EnchantmentSlot(slotEnch[i]), true);
            }
        }
    }

    ChatHandler chathandle = ChatHandler(player->GetSession());

    uint8 loc_idx = player->GetSession()->GetSessionDbLocaleIndex();
    const ItemTemplate* temp = item->GetTemplate();
    std::string name = temp->Name1;
    if (ItemLocale const* il = sObjectMgr->GetItemLocale(temp->ItemId))
        ObjectMgr::GetLocaleString(il->Name, loc_idx, name);

    if (slotRand[2] != -1)
        chathandle.PSendSysMessage("Newly acquired |cffFF0000 %s |rwas found with |cffFF0000 3 |rextra enchantments! (Rarity Rolls: %d, %d, %d; Enchantment Tiers: %u, %u, %u)", name, rarityRoll[0], rarityRoll[1], rarityRoll[2], tier[0], tier[1], tier[2]);
    else if (slotRand[1] != -1)
        chathandle.PSendSysMessage("Newly acquired |cffFF0000 %s |rwas found with |cffFF0000 2 |rextra enchantments! (Rarity Rolls: %d, %d; Enchantment Tiers: %u, %u)", name, rarityRoll[0], rarityRoll[1], tier[0], tier[1]);
    else if (slotRand[0] != -1)
        chathandle.PSendSysMessage("Newly acquired |cffFF0000 %s |rwas found with |cffFF0000 1 |rextra enchantment! (Rarity Roll: %d; Enchantment Tier: %u)", name, rarityRoll[0], tier[0]);
}

std::tuple<uint32, int, uint8> getRandEnchantment(Item* item)
{
    uint32 itemClass = item->GetTemplate()->Class;
    uint32 itemQuality = item->GetTemplate()->Quality;
    std::string classQueryString = "";
    int rarityRoll = -1;
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

    switch (itemQuality)
    {
        case GREY:
            rarityRoll = rand_norm() * 25;
            break;
        case WHITE:
            rarityRoll = rand_norm() * 50;
            break;
        case GREEN:
            rarityRoll = 45 + (rand_norm() * 20);
            break;
        case BLUE:
            rarityRoll = 65 + (rand_norm() * 15);
            break;
        case PURPLE:
            rarityRoll = 80 + (rand_norm() * 14);
            break;
        case ORANGE:
            rarityRoll = 93;
            break;
    }

    // resolve negative rarity roll
    if (rarityRoll < 0)
        return std::make_tuple(0, 0, 0);

    if (rarityRoll <= 44)
        tier = 1;
    else if (rarityRoll <= 64)
        tier = 2;
    else if (rarityRoll <= 79)
        tier = 3;
    else if (rarityRoll <= 92)
        tier = 4;
    else
        tier = 5;

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
