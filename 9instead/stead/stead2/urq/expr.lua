LEFT=0
RIGHT=1
FUNC=-2
PAR=-1
function op(pri, dir, na, mna, f, regex)
	return { pri = pri, dir = dir, nr = na, mnr = mna, f = f, reg = regex};
end

function get_string(a)
--	print ("get string for:'"..a.."'");
--	if is_string(a) then
--		return a:gsub('^"',""):gsub('"$',"");
--	end
	if is_string_pattern(a) then
		return string_pattern(a);
	end
	if not is_number(a) then
		return tostring(a)
	end
	return nil;
end

function from_strings(a, b, nt)
	local aa = get_string(a);
	local bb = get_string(b);
	if aa and not nt then
		a = aa:len();
	end
	if bb and not nt then
		b = bb:len();
	end
	if not a or not b then
		error ("Error in line: "..urq.ip)
	end
	return tonumber(a),tonumber(b)
end

op_or = function(a, b)
	a,b = from_strings(a, b)
	if a ~=0 or b ~=0 then
		return 1
	end
	return 0
end
op_and = function(a, b)
	a,b = from_strings(a, b)
	if a ~= 0 and b ~= 0 then
		return 1
	end
	return 0
end
op_div = function(a, b)
	a,b = from_strings(a, b)
	return tonumber(a / b);
end
op_mul = function(a, b)
--	print("op_plus: ", a, b);
	a,b = from_strings(a, b)
	return tonumber(a * b);
end
op_plus = function(a, b)
--	print ("plus:",a,b);
	local aa,bb
	if urq.extension_furq then
		aa = get_string(a);
		bb = get_string(b);
		if aa and bb then
--			return tostring(aa)..tostring(bb);
			table.insert(urq.strings, tostring(aa..bb));
			return "__urq_string__"..tostring(table.maxn(urq.strings));
		end
	end
	if not b then
		return tonumber(a)
	end
	a,b = from_strings(a, b)
	return tonumber(a + b);
end
op_minus = function(a, b)
--	print("op_plus: ", a, b);
	a,b = from_strings(a, b)
	if not b then
		return tonumber(-a)
	end
	return tonumber(a - b);
end
-- "a*==
op_seq = function(a, b)
	local aa,bb
	if urq.extension_strings then
		aa = get_string(a);
		bb = get_string(b);
		if aa and bb then
			aa = tolow(tostring(aa)):gsub("[*]",".*"):gsub("[?]",".?");
			bb = tolow(tostring(bb)):gsub("[*]",".*"):gsub("[?]",".?");
			if aa:find("^"..bb.."$") or bb:find("^"..aa.."$") then
				return 1
			end
		end
	end
	return op_eq(a, b); -- ok, is it hack?
end

op_eq = function(a, b)
	local aa,bb
	if urq.extension_strings then
		aa = get_string(a);
		bb = get_string(b);
--		print ("a =",a," b = ", b,"aa = ",aa,"bb =",bb);
		if aa and bb then
			if tolow(tostring(aa)) == tolow(tostring(bb)) then
				return 1
			end
			return 0
		end
	end
	a,b = from_strings(a, b, not get_string(b)) -- exception???
	if a == b then
		return 1
	end
	return 0;
end
op_lt = function(a, b)
	a,b = from_strings(a, b)
--	print (a,"<",b);
	if a < b then
		return 1
	end
	return 0;
end

op_le = function(a, b)
	a,b = from_strings(a, b)
	if a <= b then
		return 1
	end
	return 0;
end

op_gt = function(a, b)
	a,b = from_strings(a, b)
	if a > b then
		return 1
	end
	return 0;
end

op_ge = function(a, b)
	a,b = from_strings(a, b)
	if a >= b then
		return 1
	end
	return 0;
end

op_ne = function(a, b)
	local aa,bb
	if urq.extension_strings then
		aa = get_string(a);
		bb = get_string(b);
--		print ("a =",a," b = ", b,"aa = ",aa,"bb =",bb);
		if aa and bb then
			if tolow(tostring(aa)) ~= tolow(tostring(bb)) then
				return 1
			end
			return 0
		end
	end
	a,b = from_strings(a, b)
	if a ~= b then
		return 1
	end
	return 0;
end

op_not = function(a)
	a,b = from_strings(a, b)
	if a ~= 0 then
		return 0
	end
	return 1;
end

operators = {
	["||"] = op(1, 0, 2, 2, op_or),-- "||"),--"or[ \t]"),
	["&&"] = op(2, 0, 2, 2, op_and),-- "&&"),--"and[ \t]"),
	["<>"] = op(3, 0, 2, 2, op_ne, "<[ \t]*>"),
	["!="] = op(3, 0, 2, 2, op_ne, "![ \t]*="),
	["~="] = op(3, 0, 2, 2, op_ne, "~[ \t]*="),
	["="] = op(4, 0, 2, 2, op_eq, "=[ \t]*"),
	["=="] = op(4, 0, 2, 2, op_seq ),
	[">="] = op(5, 0, 2, 2, op_ge, ">[ \t]*="),
	[">"] = op(5, 0, 2, 2, op_gt, ">"),
-- n onj
	["<="] = op(5, 0, 2, 2, op_le, "<[ \t]*="),
	["<"] = op(5, 0, 2, 2, op_lt, "<"),
	["-"] = op(6, 0, 2, 1, op_minus),
	["+"] = op(6, 0, 2, 1, op_plus),
	["/"] = op(7, 0, 2, 2, op_div),
	["*"] = op(7, 0, 2, 2, op_mul),
-- unar -
-- unar +
	["!"] = op(8, 0, 1, 1, op_not), --, "!"), --"not[ \t]"),
	["("] = op(PAR, 0),
	[")"] = op(PAR, 0),
}

function is_op(s)
	if operators[s] and operators[s].pri > 0 then
		return true;
	end
	return false
end

function is_token(s)
	if operators[s] then
		return true;
	end
	return false
end

function stack()
	local v = {}
	v.push = function(s, v)
		table.insert(s, v);
	end
	v.pop = function(s, v)
		return table.remove(s, table.maxn(s));
	end
	v.top = function(s)
		return s[table.maxn(s)]
	end
	return v;
end

function find_op(s)
	local k,i,n,o,oper
	local resi
	local resk
--	print (s);
	for k,o in pairs(operators) do
		if o.reg then
			i,n = s:find(o.reg, 1);
		else
			i,n = s:find(k, 1, true);
		end
		if not resi then resi = i; resk = n; oper = k; end
		if i and ((i < resi) or (i == resi and k:len() > oper:len())) then
			resi = i
			resk = n
			oper = k
--			print ("selecting "..oper.." at "..i);
		end
	end
	return resi,resk,oper
end

function get_token(s)
	local i,k,oper
	if not s then
		return
	end
	s = s:gsub("^[ \t]+","");
	if s:find("^[ \t]*$") then
		return nil,nil
	end
	local i,k,oper = find_op(s);
	if not i then
		return s:gsub("[ \t]+$", ""), nil;
	end
	if i == 1 then
		return oper, s:sub(k+1)
	end
	return s:sub(1, i - 1):gsub("[ \t]+$",""), s:sub(i);
end

function expr2rpn(s)
	local t,lastt
	opstack = stack {};
	output = stack {};

	while true do
		t,s = get_token(s);

		if t == '-' and (is_op(lastt) or lastt == nil) then -- hack for unar -
			output:push('-1');
			t = '*'
		end
		if t ~= '(' and t ~= ')' then
			lastt = t;
		end
		if not t then -- no token
			while opstack:top() do
				local t = opstack:pop();
				if t == "(" or t == ")" then
					return nil
				end
				output:push(t);
			end
			return output;
		end
		if t == '(' then
			opstack:push(t);
		elseif t == ')' then
			while opstack:top() and opstack:top() ~= '(' do
--				print ("push ",opstack:top());
				output:push(opstack:pop());
				if opstack:top() and operators[opstack:top()].pri == FUNC then
					output:push(opstack:pop());
				end 
			end
			if opstack:top() ~= '(' then
				return nil
			end
			opstack:pop();
		elseif operators[t] == nil then
--			print ("pushx ",t);
			output:push(t);
		elseif operators[t].pri == FUNC then
			opstack:push(t);
		else
			local o1 = operators[t];
			while is_op(opstack:top()) do
				local o2 = operators[opstack:top()];
--				print (opstack:top());
				if (o1.dir == LEFT and o1.pri <= o2.pri) 
				or (o1.dir == RIGHT and o1.pri < o2.pri) then
--					print ("pusho ",opstack:top());
					output:push(opstack:pop());
				else
					break
				end
			end
			opstack:push(t);
		end
	end
end
function is_number(s)
	if not s then
		return false
	end
	if tonumber(s) then
		return true
	end
	return false
end
function rpnexpr(t)
	local top = 1
	while t ~= nil and top <= table.maxn(t) do
		if is_op(t[top]) then
			local o = table.remove(t, top);
			local oper = operators[o];
			local n = oper.nr;
			local f = oper.f;
			top = top - 1;
--			print("got ",o, "top ", top);
			if top < n then
				if oper.mnr and top >= oper.mnr then
					n = top;
				else
				-- no enouth args
					--error "no enouth args";
					return nil
				end
			end
			local a = {}
			local i = top - n + 1;
			while n ~= 0 do
				if not t[i] then
					error ("Error in expression in line: "..urq.ip);
				end
				if is_number(t[i]) then
					t[i] = tonumber(t[i]);
				end
				table.insert(a, t[i]);
				table.remove(t, i);
				n = n - 1;
			end
			local r = f(unpack(a));
			if r == nil then
				return nil -- wrong oper
			end
			table.insert(t, i, r);
			top = i + 1;
		--	print ("table: ", table.maxn(t));
		else
			top = top + 1;
		end
	end
	if t == nil or table.maxn(t) < 1 then
		return nil -- no result
	end
	if table.maxn(t) > 1 then
		return nil -- multi?
	end
	return t[1];
end
function expr(s)
	local t = expr2rpn(s);
--	for k in ipairs(t) do
--		print(t[k]);
--	end
	local r = rpnexpr(t);
	local t = get_string(r);
	if t then
		r = tostring(t)
	else
		r = tonumber(r);
	end
--	print ("Result: "..r.." Type:"..type(r))
	return r
end
--[[
t = expr2rpn ("(0=5) and (2>=1)");
for k in ipairs(t) do
	print(t[k]);
end
print (rpnexpr(t))
--]]
-- vim:ts=4
