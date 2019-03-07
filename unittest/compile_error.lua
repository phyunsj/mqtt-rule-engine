function t_filter(a) -- temperature
    if type(tonumber(a)) == "number" then
        local temperature = math.floor(tonumber(a))
        if temperature < 10 or temperature2 > 100 then 
            return  1, "{ 'temperature' : "..tostring(temperature).." }"
        else
            return  0, "{ 'temperature' : "..tostring(temperature).." }"
        end
    else
        return 0, "{ 'temperature' : 0 }"
    end 
    return 0, "{ 'temperature' : 'should not be reached here' }"
end