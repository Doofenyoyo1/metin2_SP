fishing_drop = {}
local fishing_drop_table = {
    -- itemVnum, itemCount, reqRodLevel, maxRodLevel, minimumBonus, poolValue
    27802, 1, 0, 5, 0, 393, 
    27803, 1, 0, 6, 0, 280, 
    27804, 1, 0, 7, 0, 186, 
    27805, 1, 0, 9, 20, 141, 
    27806, 1, 0, 9, 20, 96, 
    27807, 1, 0, 9, 20, 141, 
    27808, 1, 1, 9, 30, 96, 
    27809, 1, 1, 9, 30, 48, 
    27810, 1, 1, 9, 30, 34, 
    27811, 1, 1, 9, 30, 34, 
    27812, 1, 1, 9, 30, 34, 
    27813, 1, 1, 9, 30, 34, 
    27814, 1, 2, 9, 40, 25,
    27815, 1, 2, 9, 40, 25, 
    27816, 1, 2, 9, 40, 38, 
    27818, 1, 2, 9, 40, 38, 
    27819, 1, 3, 9, 50, 15, 
    27820, 1, 2, 9, 50, 15, 
    27821, 1, 2, 9, 50, 15, 
    27822, 1, 3, 9, 50, 15, 
    27823, 1, 4, 9, 70, 6, 
    70201, 1, 1, 6, 20, 23, 
    "farba", 1, 1, 6, 20, 80, 
    70007, 1, 2, 9, 40, 73, 
    70051, 1, 1, 9, 40, 90, 
    70050, 1, 1, 9, 40, 51, 
    50002, 1, 1, 9, 30, 51, 
    70049, 1, 2, 9, 50, 37, 
    70048, 1, 2, 9, 40, 51, 
    70102, 1, 4, 9, 60, 42, 
    50009, 1, 2, 9, 40, 42, 
    50008, 1, 3, 9, 50, 34, 
    70006, 1, 2, 9, 40, 42, 
    27798, 1, 5, 9, 50, 25, 
    -- 80008, 1, 5, 9, 80, 2, -- usuwamy brylke z dropu
    25040, 1, 5, 9, 80, 5, 
    30370, 1, 6, 9, 100, 1, 
    -- 71282, 1, 6, 9, 80, 3, 
    30373, 1, 6, 9, 90, 3, 
    30374, 1, 6, 9, 100, 1, 
    71025, 1, 6, 9, 90, 2, 
    71026, 1, 6, 9, 90, 3,
}

local fishing_drop_data_length = 6

local function get_drop_by_index(i)
    local indexInDropTable = (i-1) * fishing_drop_data_length + 1

    local itemVnum = fishing_drop_table[indexInDropTable]
    local name = ""
    if itemVnum == "farba" then
        itemVnum = number(70202, 70206)
        name = locale.fishing.hair_dye
    else
        name = item.proto_get_name(itemVnum)
    end

    return {
        index = i,
        name = name,
        itemVnum = itemVnum,
        itemCount = fishing_drop_table[indexInDropTable+1],
        reqRodLevel = fishing_drop_table[indexInDropTable+2],
        maxRodLevel = fishing_drop_table[indexInDropTable+3],
        minimumBonus = fishing_drop_table[indexInDropTable+4],
        poolValue = fishing_drop_table[indexInDropTable+5],
    }
end

local function get_total_pool_value(rodLevel, bonus)
    local drop_table_length = table.getn(fishing_drop_table)

    local pool = 0
    for i in range(1, drop_table_length, fishing_drop_data_length) do
        local minLevel = fishing_drop_table[i + 2]
        local maxLevel = fishing_drop_table[i + 3]
        local minBonus = fishing_drop_table[i + 4]
        if rodLevel >= minLevel and rodLevel <= maxLevel and bonus >= minBonus then
            pool = pool + fishing_drop_table[i + 5]
        end
    end
    return pool
end

local function get_random_drop(rodLevel, bonus)
    local drop_table_length = table.getn(fishing_drop_table)

    local maxPool = get_total_pool_value(rodLevel, bonus)
    local poolCheck = 0
    local luckyNumber = number(1, maxPool)
    debug("fishing drop rodLevel: %d, bonus: %d, maxPool: %d, luckyNumber: %d", rodLevel, bonus, maxPool, luckyNumber)
    for i in range(1, drop_table_length, fishing_drop_data_length) do
        local minLevel = fishing_drop_table[i + 2]
        local maxLevel = fishing_drop_table[i + 3]
        local minBonus = fishing_drop_table[i + 4]
        if rodLevel >= minLevel and rodLevel <= maxLevel and bonus >= minBonus then
            local nextPoolCheck = poolCheck + fishing_drop_table[i + 5]
            if luckyNumber > poolCheck and luckyNumber <= nextPoolCheck then
                local index = math.floor(i / fishing_drop_data_length + 1)
                debug("fishing drop index: %d", index)
                return get_drop_by_index(index)
            end
            poolCheck = nextPoolCheck
        end
    end
    return nil
end


local function open_fish(fishVnum)
    local fishPool = 0
    local length = table.getn(fishing_drop_table)
    for i in range(1, length, fishing_drop_data_length) do
        if fishing_drop_table[i] == fishVnum then
            fishPool = fishing_drop_table[i + fishing_drop_data_length - 1]
        end
    end
    if fishPool == 0 then
        syschat("Wystapil blad (kod bledu: fish_no_pool_found)")
        return
    end

    local drop_from_fish_table = {
        27987, 1, 8,
        27799, 1, 28,
        "water", 1, 47,
        "dead", 1, 150,
        27801, 1, 20,
    }

    local maxPool = 280
    local minPool = 6
    local checkPool = fishPool - minPool
    local deltaPool = maxPool - minPool
    local progress = minmax(0, 1-(checkPool / deltaPool), 1)
    local addCalmChance = math.floor(10 * progress)
    local calmChance = drop_from_fish_table[3] + addCalmChance
    drop_from_fish_table[3] = calmChance
    -- debug("checkPool %d, deltaPool %d progress %d, addCalmChance %d", checkPool, deltaPool, progress, addCalmChance)
    debug("additional calm chance (%d%%)", addCalmChance)

    local i = get_random_element_from_pool_table(drop_from_fish_table, 3)
    local itemVnum = drop_from_fish_table[i]
    local itemCount = drop_from_fish_table[i+1]
    if itemVnum == "water" then
        syschat(get_lctext("The fish vanished in the depths of the water."))
    elseif itemVnum == "dead" then
        pc.give_item2(fishVnum+30)
    elseif itemVnum == 27801 then
        syschat(get_lctext("There is a Worm inside the Fish."))
    elseif itemVnum == 27987 then
        syschat(get_lctext("There is a Clam inside the Fish."))
    end

    if type(itemVnum) == "number" then
        pc.give_item2(itemVnum, itemCount)
    end
end

fishing_drop.get_random_drop = get_random_drop
fishing_drop.get_drop_by_index = get_drop_by_index
fishing_drop.open_fish = open_fish