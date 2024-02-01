#pragma once
#include "api/APIHelp.h"

#include <string>


//////////////////// Classes ////////////////////
class ItemStack;

class ItemClass : public ScriptClass {
private:
    std::variant<std::unique_ptr<ItemStack>, ItemStack*> item;

    // Pre data
    std::string name, type;
    int         id, count, aux;

public:
    explicit ItemClass(ItemStack* p, bool isNew);
    void preloadData();

    ItemStack* get() {
        if (std::holds_alternative<std::unique_ptr<ItemStack>>(item)) {
            return std::get<std::unique_ptr<ItemStack>>(item).get();
        } else {
            return std::get<ItemStack*>(item);
        }
    }

    static Local<Object> newItem(ItemStack* p);
    static ItemStack*    extract(Local<Value> v);
    Local<Value>         asPointer(const Arguments& args);

    Local<Value> getName();
    Local<Value> getType();
    Local<Value> getId();
    Local<Value> getCount();
    Local<Value> getAux();
    Local<Value> getDamage();
    Local<Value> getAttackDamage();
    Local<Value> getMaxDamage();
    Local<Value> getMaxStackSize();
    Local<Value> getLore();

    Local<Value> isArmorItem();
    Local<Value> isBlock();
    Local<Value> isDamageableItem();
    Local<Value> isDamaged();
    Local<Value> isEnchanted();
    Local<Value> isEnchantingBook();
    Local<Value> isFireResistant();
    Local<Value> isFullStack();
    Local<Value> isGlint();
    Local<Value> isHorseArmorItem();
    Local<Value> isLiquidClipItem();
    Local<Value> isMusicDiscItem();
    Local<Value> isOffhandItem();
    Local<Value> isPotionItem();
    Local<Value> isStackable();
    Local<Value> isWearableItem();

    Local<Value> set(const Arguments& args);
    Local<Value> clone(const Arguments& args);
    Local<Value> isNull(const Arguments& args);
    Local<Value> setNull(const Arguments& args);
    Local<Value> setAux(const Arguments& args);
    Local<Value> setLore(const Arguments& args);
    Local<Value> setDisplayName(const Arguments& args);
    Local<Value> setDamage(const Arguments& args);
    Local<Value> getNbt(const Arguments& args);
    Local<Value> setNbt(const Arguments& args);

    Local<Value> match(const Arguments& args);
};

extern ClassDefine<ItemClass> ItemClassBuilder;