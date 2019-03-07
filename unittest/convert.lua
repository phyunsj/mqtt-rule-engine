function action(a)
    if type(tonumber(a)) == "number" then
        local humidity = tonumber(a)
        humidity = math.floor(math.abs(humidity / 2048 * 100 ))
        return  1, "{ 'humidity' : "..tostring(humidity).." }"
    else
        return  0, "{ 'humidity' : 0 }"
    end
end   