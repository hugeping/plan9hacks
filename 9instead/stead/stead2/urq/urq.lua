if stead.version < "1.4.0" then
	error [[Для игры нужен INSTEAD версии не ниже, чем 1.4.0
	http://instead.googlecode.com]]
end

require ("urq/expr")
require ("urq/xlat866")
require ("urq/xlat1251")
require ("urq/xlatutf8")
require ("urq/kbd-win")
require ("urq/kbd-dos")
require ("urq/kbd-en")
require ("urq/kbd-utf8")

urq = {
	version = "2.0",
	extension_strings = true,
	extension_furq = true,
	extension_global_else = false,
	extension_input = true,
	extension_useinv = false, 
	extension_varimage = true, 
	extension_loccnt = true,
	extension_loccnt_btn = true,
	extension_invproc = false,
	extension_print_spaces = false,
	extension_pause_btn = true,
	extension_input_btn = false,
	cursor = '_',
	lastcmd = 'go',
	last_com = 'common',
	examine = "Осмотреть",
	selector = ">> ",
	nam = 'urq',
	object_type = true,
	eol = "\n",
	default_delim = ' \t,"?!',
	ip = 1,
	pause_ip = 0,
	pause_depth = 1,
	pause_stack = {},
	lastip = 1,
	debug = false,
	paused = false,
	quest = { n = 0 },
	strings = { },
	labels = { },
	orig_labels = { },
	use_labels = { },
	output = '',
	last_output = '',
	cur_loc = '',
	branches = { n = 0 },
	ibranch = 0,
	stack = { },
	vars = { ["fp_prec"] = 2, ["urq_instead"] = 2.0 },
	str = { n = 0 },
	inv = { n = 0 },
	invo = { n = 0 },
	save = function(self, name, h, need)
		if self.last then
			savevar(h, self.last, name..".last", true)
		end
		if self.last_loc then
			savevar(h, self.last_loc, name..".last_loc", true)
		end
		if self.last_act then
			savevar(h, self.last_act, name..".last_act", true)
		end
		if self.input then
			savevar(h, self.input, name..".input", true)
		end
		savevar(h, self.last_output, name..".last_output", true)
		savevar(h, self.lastcmd, name..".lastcmd", true)
		savevar(h, self.last_com, name..".last_com", true)
		savevar(h, self.lastip, name..".lastip", true)
		savevar(h, self.pause_ip, name..".pause_ip", true)
		savevar(h, self.pause_depth, name..".pause_depth", true)
		savevar(h, self.paused, name..".paused", true)
		savevar(h, self.ibranch, name..".ibranch", true)
		savevar(h, self.branches, name..".branches", true)
		savevar(h, self.stack, name..".stack", true)
		savevar(h, self.pause_stack, name..".pause_stack", true)
		savevar(h, self.inv, name..".inv", true)
		savevar(h, self.invo, name..".invo", true)
		savevar(h, self.vars, name..".vars", true)
		savevar(h, self.str, name..".str", true)
		savevar(h, self.cur_loc, name..".cur_loc", true)
	end
}

function tolow(s)
	if not s then
		return
	end
	s = s:lower();
	local xlat;
	if game.codepage == "cp1251" or game.codepage == "CP1251" then
		xlat = lowerwin;
	elseif game.codepage == "cp866" or game.codepage == "CP866" then
		xlat = lowerdos;
	else
		xlat = lowerutf8;
	end
	if xlat then
		local k,v
		for k,v in pairs(xlat) do
			s = s:gsub(k,v);
		end
	end
	return s;
end


function take_label(line)
	line = line:gsub("^:([^&]*)$","%1"):gsub("^[ \t]+",""):gsub("[ \t]+$","");
	return label(line);
end

function decode_qs1(l)
	local i
	local r=''
	for i=1,l:len() do
		local b = l:byte(i,i)
		if b >= 32 then
			b = 287 - b;
		end
		r = r..string.char(b);
	end
	return r
end

function lurq(name)
	local line,cline
	local incomment
	local file
	if get_gamepath then
		file = io.open(get_gamepath()..'/'..name, "r");
	else
		file = io.open(name, "r");
	end
	if not file then
		return false
	end

	if name:find("qs1$") then
		urq.qs1 = true
	end

	for line in file:lines() do
		if urq.qs1 then
			line = decode_qs1(line);
		end
		line = line:gsub("\r","");
		table.insert(urq.quest, line);
		line = line:gsub("^[ \t]+","");
		line, incomment = strip_comments(line, incomment);
		if line:find("^:") then
			line,cline = take_label(line);
			if urq.labels[line] == nil then
				urq.labels[line] = table.maxn(urq.quest);
				urq.orig_labels[line] = cline;
				if line:find("use_",1, true) then
					urq.use_labels[line] = urq.labels[line];
				end
--				print ("label added:", line, cline, table.maxn(urq.quest));
			end
		elseif line:lower():find("^[ \t]*include[ \t]") then
			local s,e = line:lower():find("include");
			local f = strip(line:lower():sub(e + 1));
			lurq(f);
		end
	end
	file:close();
	if game.codepage == "cp866" or game.codepage == "CP866" then -- hack for 866 cp
		urq.examine = '�ᬮ����';
	end
end

function label(s)
	if s == nil then
		return nil
	end
	s = strip(s);
	return tolow(s),s
end

local function urqlink(loc, str)
	local lloc
	if loc == nil or str == nil then
		error ("Error in link at line: "..urq.ip);
	end
	local loc_link = false 

	if type(loc) == 'string' and loc:find("^[ \t]*[!%%]") then
		loc = loc:gsub("^[!%%][ \t]*", "")
		loc_link = true
	end

	if str:find('^[ \t]*$') then str = '...' end

	loc, lloc  = urqargs(loc);

	if loc_link and lloc then
		lloc = '!'..lloc
	end
	if not urq.labels[loc] then
		return "<a:nop>"..str.."</a>";
	end
	return "<a:go "..lloc..","..strip(tostring(str)):gsub(">","\\>")..">"..str.."</a>";
end

local function do_links(s)
	s = s:gsub("%[%[[^%]|]+%]%]", function(s)
		local l = s:gsub("^%[%[", ""):gsub("%]%]$","");
		if urq.labels[l] then
			return "[["..l.."|"..l.."]]"
		else
			return s
		end
	end)
	s = s:gsub("%[%[[^%]|]+|[^%]]+%]%]", function(s)
		s = s:gsub("^%[%[", ""):gsub("%]%]$", "");
		local a = s:gsub("([^|]+)|.+", "%1");
		local b = s:gsub("[^|]+|(.+)", "%1");
		local l,t
		local s, e = b:find("^[^((]+[((].*[))],");
		if e then l = b:sub(1, e - 1); t = b:sub(e + 1); end
		if not l then
			l = b;
		end
		s = urqlink(l, a);
		return s
	end)
	return s
end
function urqprint(str, nl)
	local pre=""
	local post=""
	if urq.paused then
		return
	end
	if urq.extension_furq and str and not str:find("^[ \t]*$") then
		local a = tonumber(urq.vars["textalign"])
		if a == 1 then
			pre = ""
			post = ""
		elseif a == 2 then
			pre = ""
			post = ""
		elseif a == 3 then
			pre = ""
			post = ""
		end
	end
	if str then
		if urq.extension_print_spaces then
			str = str:gsub("^[ \t]+", function(s)
				return "<w:"..s:gsub("\t","        ")..">"
			end)
		end
		if urq.extension_furq then
			str = do_links(str)
		end
		urq.output = urq.output..pre..str..post;
	end
	if nl then
		urq.output = urq.output..urq.eol;
	end
end

function urqbtnargs(l)
	local n = 0;
	local a, num
	if urq.extension_furq then
		num = l:gsub("^[^;]+;?([0-9]*)$", "%1");
		if tonumber(num) then
			l = l:gsub(";[0-9]*$", "");
			a = urq.str[l.."__urq_btn_arguments__"..tonumber(num)];
		end
		clearbtnargs(l);
	end

	if not a or urq.ro or urq.paused then
		return l
	end

	urq.str[l.."__urq_btn_arguments__"..tonumber(num)] = nil

	local function subst(s)
		local r
		s = strip(s);
		s = s:gsub('\003', ',');
		r = urqexpr(s, false, true);
		n = n + 1
		if type(r) == 'string' then
			urq.str[l.."_"..tostring(n)] = r;
		else
			urq.vars[l.."_"..tostring(n)] = r;
		end
	end

	local function reps(s)
		return s:gsub(',','\003');
	end
	if urq.extension_strings then
		a = string.gsub(a, '"[^"]*"', reps);
	end
	a:gsub("[^,]+", subst);
	return l, n
end

function clearbtnargs(loc)
	local k,v
	if urq.paused or urq.ro then return end
	for k,v in pairs(urq.vars) do
		if k:find("^"..loc.."_".."[0-9]+") then
			urq.vars[k] = nil;
		end
	end
	for k,v in pairs(urq.str) do
		if k:find("^"..loc.."_".."[0-9]+") then
			urq.str[k] = nil;
		end
	end
end

function urqargs(loc)
	local lloc
	local loca = 0
	loc,lloc = label(loc);
	if urq.extension_furq and not urq.labels[loc] then
		local a = lloc:gsub("^[^((]+[((](.*)[))]$","%1")
		loc = loc:gsub("^([^(]+)[((].*[))]$","%1")
		loc = strip(loc)
--		clearbtnargs(loc);
		loca = tostring(urq.btn_args)
		if a and not urq.paused and not urq.ro then
			urq.str[loc.."__urq_btn_arguments__"..loca] = a;
		end
		urq.btn_args = urq.btn_args + 1
		return loc, loc..';'..loca
	end
	return loc, loc;
end

function urqbtn(loc, str)
	local lloc
	if loc == nil or str == nil then
		error ("Error in btn at line: "..urq.ip);
	end
	if str:find('^[ \t]*$') then str = '...' end
	loc, lloc  = urqargs(loc);
	return ""..urq.selector.."<a:go "..lloc..","..strip(tostring(str)):gsub(">","\\>")..">"..str.."</a>\n";
end

function urqend(s)
	return urq.output;
end

function urqif(s)
--	print (" * if: '"..s.."'");
	urq.ibranch = urq.ibranch + 1
	if urq.paused then
		local r = urq.branches[urq.ibranch];
		return r
	end
	s = strip(s);
	local r = urqexpr(s, true)
	if r == true or (tonumber(r) ~= nil and tonumber(r) ~= 0) then
		r = true
	else
		r = false
	end
--	print (" * branches: "..urq.ibranch..":"..tostring(r));
	table.insert(urq.branches, r);
	return r
end
function debuginv()
	local i,k
	for i,k in pairs(urq.inv) do
		print("'"..i.."'");
	end
end

function invoidx(a)
	local k,v
	for k,v in ipairs(urq.invo) do
		if v == tolow(strip(a)) then
			return k
		end
	end
	return nil
end

function invodel(a)
	local i = invoidx(a);
	if not i then
		return
	end
	table.remove(urq.invo, i)
end

function invoadd(a)
	table.insert(urq.invo, strip(a))
end

function urqinvkill(a)
	if urq.paused or urq.ro then return end
	if a then
		a = tolow(strip(tostring(a)));
		urq.inv[a] = nil;
		urq.vars["inv_"..a] = nil;
		invodel(a);
		return
	end
	local k,v
	for k,v in pairs(urq.inv) do
		urq.inv[k] = nil;
		urq.vars["inv_"..k] = nil;
		invodel(k);
	end
end

function urqperkill()
	local k,v
	if urq.paused or urq.ro then return end
	for k,v in pairs(urq.vars) do
		if not k:find("^inv_") then
			urq.vars[k] = nil;
		end
	end
	for k,v in pairs(urq.str) do
		if not k:find("^current_loc$") and not k:find("^previous_loc$") and not k:find("^last_btn_caption$") then
			urq.str[k] = nil;
		end
	end
	urq.str["tokens_delim"] = urq.default_delim
	urq.vars["fp_prec"] = 2
	urq.vars["urq_instead"] = 2.0
end


function urqinv(o,n)
	if urq.paused or urq.ro then return end
	if n == nil or o == nil then
		error ("Error in line: "..urq.ip)
	end
	n = tonumber(n)
	local ob = strip(o);
	o = tolow(ob);
	if urq.inv[o] == nil then -- make new
		if n <= 0 then
			return
		end
		urq.inv[o] = { n = n, name = ob };
		urq.vars["inv_"..o] = n;
		invoadd(o);
--		print ("added inv: '"..o.."'",urq.inv[o]);
		return
	end
	local i = urq.inv[o];
	i.n = i.n + n;
	urq.vars["inv_"..o] = i.n;
	if i.n <= 0 then
		urq.inv[o] = nil
		urq.vars["inv_"..o] = nil;
		invodel(o);
	end
	return
end

function get2args(a)
	local l = a:gsub("^([^,]+),.*$","%1");
	local t
	if a:find(",") then
		t = a:gsub("^[^,]*,(.*)$","%1");
	end
	return l,t
end

function urqlt(a, b)
	if a < b then
		return 1
	end
	return 0
end

function urqle(a, b)
	if a <= b then
		return 1
	end
	return 0
end

function urqeq(a, b)
	if a == b then
		return 1
	end
	return 0
end

function urqbool(a, b)
	if a then
		return 1
	end
	return 0
end

function is_string(s)
	if not s then return false end
	if tostring(s):find('^[ \t]*"[^"]*"[ \t]*$') then
		return true
	end
	return false
end

function is_string_pattern(s)
	if not s then return false end
	if tostring(s):find("^__urq_string__[0-9]+") then
		return true
	end
	return false
end

function string_pattern(s)
	if not s then return '' end
	return tostring(urq.strings[tonumber(s:sub(15))]);
end
function fprec(n)
	local prec = urq.vars["fp_prec"];
	if not tonumber(n) then
		error ("Error in expr at line: "..urq.ip);
	end
	n = tonumber(string.format("%."..prec.."f", tonumber(n)));
	return tonumber(n)
end

function check_inv(s)
	if urq.inv[s] then
		return 1
	end
	local n = s:gsub("^([^ \t]+)[ \t][^ \t]+$", "%1");

	local i = s:gsub("^[^ \t]+[ \t]([^ \t]+)$", "%1");

	if is_number(n) and i then
		if urq.inv[i] then i = urq.inv[i].n else i = 0 end
		return tostring(n).."<="..tostring(i);
	end
	return nil
end

function urqvar(v, inif, str) -- only numeric!
	local n
	local s = tolow(strip(v));
--	print ("urqvar for: ",v,inif);
	if s:find("^rnd[0-9]*$") then
		n=s:sub(4)
		if n and tonumber(n) then
			n = math.random(n);
		else
			n = math.random();
		end
		return fprec(n);
	elseif s == "time" then
		n = os.date('*t')
		n = os.time(n)
		return fprec(n);
	else
		if urq.vars[s] ~= nil then
			n = urq.vars[s];
		elseif urq.str[s] ~= nil then
			n = urq.str[s];
		elseif inif then
			local r = check_inv(s);
			if r ~= nil then 
				return r 
			end
		end
	end
	if n == nil then
		return 0
	end
	if urq.vars[s] == nil then
		n = tostring(n):len();
	end
	return fprec(n);
end

function urqstr(s)
	if not s then
		error ("Error in line: "..urq.ip);
	end
	s = tolow(strip(s));
--	print ("urqstr("..s..")");
	if not urq.str[s] then
		return ''
	end
	return tostring(urq.str[s]);
--	return "urqvar("..s..")";
end

function add_string(r)
	table.insert(urq.strings, r)
--	print ("added string at '".."__urq_string__"..table.maxn(urq.strings).."','"..r.."'");
	return "__urq_string__"..table.maxn(urq.strings);
end

function urqexpr(str, inif) -- already substed
--	print ("expr = '"..str.."'");
	urq.strings = {};
	str = strip(str);
	local function reps(s)
		s = s:gsub('^[ \t]*"',''):gsub('"[ \t]*$','');
		return add_string(s)
	end
	local function rep(s)
		s = strip(s);
		if s:find("^[ \t]*$") then
			return s;
		end
		if is_string_pattern(s) then
			return s;
		end
		if s:lower() == "&&" or s:lower() == '||' or s:lower() == '!' then
			return s
		end
		if is_number(s) then
			return s -- number as is
		end
		if urq.str[tolow(s)] then
			return add_string(urq.str[tolow(s)]);
		end
		return urqvar(s, inif); -- last
	end
--	function reps2(s)
--		if s:find("^__urq_string__") then
--			return strings[tonumber(s:sub(15))];
--		end
--	end
	if urq.extension_strings then
		str = string.gsub(str, '"[^"]*"', reps);
--		print ("str after subst '"..str..'"');
	end
	str = str:lower():gsub("([ \t))((])and([ \t(())])","%1(&&)%2"):gsub("([ \t))((])or([ \t(())])","%1(||)%2"):gsub("([ \t))((])not([ \t(())])","%1(!)%2"):gsub("^([ \t]*)not([ \t((])","%1(!)%2");
--	print ("sunst1 is ='"..str.."'");
	str = string.gsub(str, "[^+=/*/>><<))((-]+", rep);
	str = string.gsub(str, "[((]||[))]", "||"):gsub("[((]&&[))]","&&"):gsub("[((]![))]","!");
--	print ("subst is ='"..str.."'");
	local r = expr(str);
--	print ("expr res ='"..r.."'");
	if not r then
		error ("Error in expression at line: "..urq.ip);
	end
	return r
end

function urqlet(l, r, str)
--	print ("let: ",l,",", r);
	if urq.paused or urq.ro then return end
	l = strip(l);
	if not str or (urq.vars["instr_leave_spc"] == nil or urq.vars["instr_leave_spc"] == 0) then
		r = strip(r);
	end
	if r == nil then
		error ("No right value at line: "..urq.ip)
	end
--	if urq.extension_strings and is_string(r) then
--		str = true
--		r = string.gsub(r,'^[ \t]*"',''):gsub('"[ \t]*$','');
--	end
	local v = tolow(l);
	if str then
		urq.str[v] = tostring(r);
		urq.vars[v] = nil;
		return
	end
	local res = urqexpr(r);
	if type(res) == 'string' then
		urq.str[v] = res;
		urq.vars[v] = nil;
	else
		urq.vars[v] = res;
		urq.str[v] = nil;
	end
--	print ("LET:", res, r)
	if v:find("^inv_") then
		local o = l:sub(5);
		if urq.inv[o] then
			urq.inv[o].n = tonumber(urq.vars[v]);
		else
			urqinv(o, tonumber(urq.vars[v]));
		end
	end
	if urq.extension_furq and v == "music_looped" then
		local m = get_music();
		if tonumber(res) == 1 then 
			res = 0
		else 
			res = 1
		end
		set_music(m, res);
	end
end

function subst(str, eval)
	local l,i,k
	local depth = 0
	local s
	if str == nil then
		return
	end
--	print (":: before subst", str, eval);
	i = str:find("[#$$]")
	while i do
		if str:sub(i, i) == "#" and (i == 1 or str:sub(i -1, i - 1) ~= "#") then
			depth = depth + 1;
			if depth == 1 then
				s = i + 1;
			end
--			print (":: start", s);
		elseif str:sub(i, i) == "$" then
			depth = depth - 1;
			if depth < 0 then
				return str -- no pairs
			end
			if depth == 0 then
--				print (":: end", i - 1);
				local r = subst(str:sub(s, i - 1), true);
				str = str:sub(1,s - 2)..r..str:sub(i + 1);
				i = s - 2 + tostring(r):len();
			end
		end
		i = str:find("[#$$]", i + 1);
--		print (":: next", i, " in ", str);
	end
	if eval then
		if str:find("^[ \t]*$") then
			str = "<w: >"
		elseif str:find("^[ \t]*/[ \t]*") then
			str = urq.eol;
		elseif str:find("^%%") then
			str = urqstr(str:sub(2));
		elseif str:find("^#") then
			local c = tonumber(strip(str:sub(2)));
			if c <= 255 then
				str = string.char(c);
			end
		else
			str = urqexpr(str);
			if type(str) == 'string' then
				str = str:len();
			end
		end
--		str = "eval("..str..")";
	end
--	print ("after subst:", str);
	return str;
end

function strip(s)
	s = tostring(s):gsub("^[ \t]+",""):gsub("[ \t]+$","");
	return s
end

function strip_comments(line, incomment)
	local s, e
	if incomment then
		s, e = line:find("*/", 1, true)
		if e then
			incomment = false;
			line = line:sub(e + 1);
		else
			line = ""
		end
	end
	while true do
		s, e = line:find("/*", 1, true)
		if not s then break end
		local ss,ee = line:find("*/", e + 1, true)
		if not ss then
			incomment = true
			line = line:sub(1, s - 1);
			break
		end
		line = line:sub(1, s - 1)..line:sub(ee + 1);
	end
	line = line:gsub("^([^;]*);.*$","%1");
	return line, incomment
end

function update_count(l)
	if urq.paused or urq.ro then return end
	if urq.extension_loccnt then
		l = "count_"..l;
	end
	if not urq.vars[l] then
		urq.vars[l] = 0;
	end
	urq.vars[l] = urq.vars[l] + 1;
end

function token_add(r, n)
	if r:find("^[ \t]*$") then
		return n
	end
	n = n + 1
	urqlet("token"..tostring(n), r, true);
	return n;
end

function find_delim(a, d)
	local se
	local i = 1
	while i <= d:len()  do
		if d == "char" then
			return 1,1
		end
		local c = d:sub(i,i);
		s = a:find(c, 1, true);
		if not se or (s and s < se) then se = s end
		i = i + 1
	end
	return se, se
end

function destroy_tokens()
	local k,v
	for k,v in pairs(urq.str) do
		if k:find("^token[0-9]+$") then
			v = nil
		end
	end
end

function urqtokens(a)
	if urq.ro or urq.paused then
		return
	end
	if not a then
		if urq.debug then
			error ("Tokens without argument in line: "..urq.ip)
		else
			print ("Tokens without argument in line: "..urq.ip)
			return
		end
	end
	local delim = urq.str["tokens_delim"];
	if not delim then delim=urq.default_delim end
	local n=0
	destroy_tokens()
	while not a:find("^[ \t]*$") do
		local r
		local s = find_delim(a, delim);
--		print ("[ + ] s",s);
		if not s then
			r = a:sub(1);
			n = token_add(r, n);
			break;
		end
		if 1 ~= s then
			r = a:sub(1, s - 1);
			n = token_add(r, n);
		elseif delim == 'char' then
			r = a:sub(1, 1);
			n = token_add(r, n);
		end
		a = a:sub(s + 1);
	end
	urq.vars["tokens_num"] = tonumber(n);
end

function update_loc(l, btn)
	if urq.paused or urq.ro then return end
	if not l then return end
	if btn then
		urq.str["previous_loc"] = urq.str["current_loc"];
		urq.str["current_loc"] = l;
		if urq.str["previous_loc"] == nil then
			urq.str["previous_loc"] = l;
		end
	end
	urq.cur_loc = l;
--	print ("[[ update loc: "..urq.str["current_loc"].."]]");
end

function findif(line)
	local s,e
	s,e = line:lower():find("[))& \t]if[ \t((]")
	if s then
		s, e = line:lower():find("if", s)
	end
	return s,e
end

function findthen(line)
	local s,e
	if not line then
		return nil
	end
	s,e = line:lower():find("[))& \t]then[ \t((]")
	if s then
		s, e = line:lower():find("then", s)
	end
	return s,e
end

function findifelse(line)
	local s, e, ss, ee
	s,e = findif(line)
	if urq.extension_global_else then
		ss,ee = line:lower():find("else")
	else
		ss,ee = line:lower():find("[$$))& \t]else[ \t((]")
	end
	if ss then ss, ee = line:lower():find("else", ss) end
	if s and ss then
		if s <= ss then return s,e, true end
		return ss,ee
	end
	if not s then return ss,ee end
	return s,e, true
end

function findelse(line)
	local s, e, i
	local pos = 0
	local inif = 0
	
	while true do
		s, e, i = findifelse(line);
		if i then 
			inif = inif + 1 
		else
			if not s then
				return nil, nil
			end
			if inif == 0 then
				return pos + s, pos + e
			end
			inif = inif - 1
		end
		line = line:sub(e + 1);
		pos = pos + e;
	end
end
function is_pause()
	local i
	local eq=true
	if urq.ip == urq.pause_ip and urq.pause_depth == urq.depth and 
		(table.maxn(urq.stack) == table.maxn(urq.pause_stack)) then
		for i=1,table.maxn(urq.stack) do
			if urq.stack[i] ~= urq.pause_stack[i] then
				eq=false
				break
			end
		end
		if eq then
			return true
		end
	end
	if urq.pause_ip == 0 then
		return true
	end
	return false
end

function pushcall()
	table.insert(urq.stack, urq.ip);
	table.insert(urq.stack, urq.depth);
end

function popcall()
	table.remove(urq.stack, table.maxn(urq.stack));
	table.remove(urq.stack, table.maxn(urq.stack));
end

function pausestack()
	local s = {}
	local i
	for i=1,table.maxn(urq.stack) do
		table.insert(s, urq.stack[i])
	end
	urq.pause_stack = s;
end

function exec(ip, tolabel)
	local line
	local ret=nil
	local incomment
	if ip == nil or ip > table.maxn(urq.quest) then
		error ("Error while exec wrong line. ")
	end

	update_loc(tolabel)

	while ip <= table.maxn(urq.quest) and not ret do
		urq.ip = ip;
		line = urq.quest[ip];
		line = line:gsub("^[ \t]+","");
		line, incomment = strip_comments(line, incomment);

		while ip < table.maxn(urq.quest) and urq.quest[ip + 1]:find("^[ \t]*_+") do -- concat lines
			ip = ip + 1;
			line = line..urq.quest[ip]:gsub("^[ \t]*_+","");
			line, incomment = strip_comments(line, incomment);
		end
		urq.ip = ip;
		urq.depth = 1
		line, incomment = strip_comments(line, incomment);
		while line ~= "" do
			line = line:gsub("^&+", ""):gsub("^[ \t]","");
			local s,e = line:find("&");
			if s then -- get line before &
				sline = subst(line:sub(1, e - 1));
			else
				sline = subst(line);
			end
			local i,k = sline:find("[ \t]");
			local cmd;
			if i then
				cmd = sline:sub(1, i - 1):gsub("^[ \t]+","");
				a = sline:sub(i + 1);
				if a:find("^[ \t]*$") then
					a = nil
				end
			else
				cmd = strip(sline);
				a = nil
			end
			if line:find("^:") then
				-- noting todo
				local l = take_label(line)
				if not urq.extension_loccnt_btn then
					update_count(l);
				end
				if urq.str["current_loc"] == nil or urq.str["previous_loc"] == nil then
					update_loc(l, true);
				end
--				print("label: "..l);
			elseif cmd:lower() == "include" then
				-- nothing todo
			elseif cmd:lower() == "inv_visible" or cmd:lower() == "html" then
				-- nothing todo
			elseif cmd:lower() == "clsb" then
				if urq.extension_input and not urq.paused then
					urq.buttons = "";
				end
			elseif cmd:lower() == "cls" or cmd:lower() == "clst" then
				if urq.extension_input and not urq.paused then
					urq.output = "";
					urq.buttons = "";
				end
				-- nothing todo
			elseif cmd:lower() == "pause" then
				if not urq.ro and urq.extension_input and is_pause() then
					if urq.paused then
						urq.paused = false
						urq.pause_ip = 0
						urq.pause_stack = {};
						timer:stop()
					else
						if a and urq.extension_furq then
							a = urqexpr(a)
						end
						if not a or tonumber(a) ~= 0 then
							urq.paused = true;
							if a == nil then a = 1000 end
							timer:set(tonumber(a));
							urq.forgetprocs = true
							ret = true;
							urq.pause_ip = urq.ip
							urq.pause_depth = urq.depth
							pausestack();
							break;
						end
					end
				end
				-- nothing todo
--			elseif cmd:lower() == "anykey" then
				-- nothing todo
			elseif cmd:lower() == "input" or cmd:lower() == "anykey" then
				if not urq.ro and urq.extension_input and is_pause() then
					if urq.paused then -- input
						urq.paused = false
						if a and urq.str[tolow(strip(a))] ~= nil then
							urqlet(a, urq.input, true);
						elseif a then
							urqlet(a, tonumber(urq.input));
						end
						urq.input = nil;
						urq.pause_ip = 0
						urq.pause_stack = {};
					else
						if cmd:lower() == "anykey" then
							urq.input = '';
						else
							urq.input = urq.cursor;
						end
						urq.paused = true
						urq.forgetprocs = true
						urq.pause_ip = urq.ip
						urq.pause_depth = urq.depth
						pausestack();
						ret = true;
						break;
					end
				end
				-- todo
			elseif cmd:lower() == "p" or cmd:lower() == "print" then
				-- a = subst(a);
				urqprint(a);
			elseif cmd:lower() == "pln" or cmd:lower() == "println" then
				urqprint(a, true);
			elseif cmd:lower() == "quit" then
				urq.forgetprocs = true; -- todo?
				ret = true
				break;
			elseif cmd:lower() == "end" then
				ret = true
				break;
			elseif cmd:lower() == "invkill" then
				urqinvkill(a);
			elseif cmd:lower() == "perkill" then 
				urqperkill();
			elseif cmd:lower() == "inv+" then
				local l,t  = get2args(a);
				if not t then 
					t = 1 
				else
					l,t = t,l
				end
				if urq.extension_furq then
					t = urqexpr(t)
				end
				urqinv(l, t);
			elseif cmd:lower() == "inv-" then
				local l,t  = get2args(a);
				if not t then 
					t = 1 
				else
					l,t = t,l
				end
				if urq.extension_furq then
					t = urqexpr(t)
				end
				if tonumber(t) == nil then
					t = 1
					print ("Warning: wrong number in inv-.");
				end
				urqinv(l, -t);
			elseif cmd:lower() == "btn" then
				local l,t
				if urq.extension_furq then
					local s, e = a:find("^[^((]+[((].*[))],");
					if e then l = a:sub(1, e - 1); t = a:sub(e + 1); end
				end
				if not l then
					l,t = get2args(a);
				end
				if urq.buttons == nil then
					urq.buttons = '';
				end
				urq.buttons = urq.buttons..urqbtn(l, t);
			elseif cmd:lower() == "forget_procs" then
				urq.forgetprocs = true;
			elseif cmd:lower() == "proc" then
				local lloc
				if a then
					a = a:gsub("[ \t]+$","");
					a, lloc = urqargs(a);
				end
				if not a or urq.labels[a] == nil then
					if not a then a = 'nil' end
					if urq.debug then
						error ("proc on non existing label '"..a.."' at line: "..ip);
					else
						print ("proc on non existing label '"..a.."' at line: "..ip);
					end
				elseif type(urq.labels[a]) == 'function' then
					a = urqbtnargs(lloc);
					urq.labels[a]();
				else
					a = urqbtnargs(lloc);
					pushcall();
					if tolabel then
						exec(urq.labels[a], a);
					else
						exec(urq.labels[a]);
					end
					popcall();
					if urq.forgetprocs then
						ret = true;
					else
						update_loc(tolabel);
					end
				end
			elseif cmd:lower() == 'tokens' then
				if a then
					a = urq.str[strip(tolow(a))];
				end
				urqtokens(a);
			elseif cmd:lower() == 'goto' then
				if a then
					local lloc
					a = a:gsub("[ \t]+$","");
					a, lloc = urqargs(a);
					if urq.labels[a] == nil then
						if urq.debug then
							error ("goto on non existing label '"..a.."' at line: "..ip);
						else
							print ("goto on non existing label '"..a.."' at line: "..ip);
						end
					else
						a = urqbtnargs(lloc);
						if tolabel then tolabel = a end
						exec(urq.labels[a], tolabel);
						ret = true
					end
				else
					if urq.debug then
						error ("goto on nil label at line: "..ip);
					else
						print ("goto on nil label '"..a.."' at line: "..ip);
					end
				end
			elseif cmd:lower() == "play" then
				if not urq.extension_furq then
					if urq.debug then
						error ("play is supported only in fireurq mode: "..urq.ip)
					else
						print ("play is supported only in fireurq mode: "..urq.ip)
					end
				end
				local m,t = get2args(a);
				set_sound(strip(m));
			elseif cmd:lower() == "image" and not sline:find("=") then
				urqprint("<g:"..tostring(strip(a)):gsub('\\','/')..">");
			elseif cmd:lower() == "save" then
				-- just nothing
				autosave(1);
			elseif cmd:lower() == "music" then
				if not urq.extension_furq then
					if urq.debug then
						error ("music is supported only in fireurq mode: "..urq.ip)
					else
						print ("music is supported only in fireurq mode: "..urq.ip)
					end
				else
					local m,t = get2args(a);
					if m == 'stop' then
						set_music('');
					else
						set_music(strip(m));
					end
				end
			elseif cmd:lower() == "else" then
				break;
			elseif cmd:lower() == "if" then
				i,k = findthen(a);
				if not i then
					if urq.debug then
						error ("if without then at line: "..ip.." ("..a..")");
					else
						print ("if without then at line: "..ip.." ("..a..")");
					end
					break
				else
					a = a:sub(1, i - 1);
					if (not urqif(a)) then
						local i,k = findelse(line);
						if i then
							s,e = i,k
						else
							break
						end
					else
						local i,k = findelse(line);
						if i then
							line = line:sub(1, i - 1);
						end
						s,e = findthen(line);
					end
				end
				--line = line:sub(e + 1);
				--print ("out", line);
			elseif cmd:lower()  == "instr" then
				if a then
					local s,e = a:find("=");
					if not s then
						error ("Error in line: "..urq.ip.." Wrong instr.")
					end
					local l = a:sub(1,s - 1)
					local r = a:sub(e + 1);
					urqlet(l, r, true);
				end
			elseif sline:find("=") then
				-- let
				local s,e = sline:find("=");
				local l = sline:sub(1,s - 1)
				local r = sline:sub(e + 1);
--				print ("let = "..l..","..r);
				urqlet(l, r);
			else
				if urq.extension_furq and (cmd:lower():find("^decor_") 
				or cmd:lower():find("^decoradd") or
				cmd:lower():find("^decordel") or 
				cmd:lower():find("^decorcol") or 
				cmd:lower():find("^decorrot") or
				cmd:lower():find("^decormov")) then
					-- just skip it
				elseif cmd~='' then
					if urq.debug then
						error ("Unknown cmd in line: "..urq.ip.." cmd: "..sline);
					else
						print ("Unknown cmd in line: "..urq.ip.." cmd: "..sline);
					end
				end
			end
			if e  then
				line = line:sub(e + 1):gsub("^[ \t]+","");
			else
				line =""
			end
			urq.depth = urq.depth + 1
		end
--		print (line.."'\n");
		ip = ip + 1
	end
	ret = urqend();
	urq.ip = ip;
	ret = ret:gsub("^[ \t]*[\n]+",""):gsub("[\n]+$","\n");
	return ret
end

function idisp(o)
	if urq.str["idisp_"..tolow(o)] ~= nil and urq.extension_furq then
		o = urq.str["idisp_"..tolow(o)];
	end
	o = o:gsub("_"," "):gsub(" ",txtnb(' '));
	return o
end

function inv()
	local k,v,k,vv,kk
	local r='';

	if urq.extension_useinv then
		for k,v in pairs(urq.use_labels) do
			if k:find("^use_inv$") then
				local d = urq.orig_labels[k];
				if not d then error ("Fatal error in: "..urq.ip) end
				if r ~= '' then r = r..stead.delim; end
				r = r..'<a:use '..k..'>'..urq.examine..'</a>';
			end
		end
	end

	for k,v in pairs(urq.use_labels) do
		local s,e = k:find("^use_inv_.*$");
		if s then 
			local d = urq.orig_labels[k];
			if not d then
				error ("Fatal error in: "..urq.ip)
			end
--			s, e = d:find("^use_inv_");
			if r ~= '' then
				r = r..stead.delim;
			end
			r = r..'<a:use '..k..'>'..idisp(d:sub(9))..'</a>';
		end
	end
	for kk,vv in ipairs(urq.invo) do
		k = tolow(strip(vv));
		v = urq.inv[k];
		if type(v) == 'table' then
			local pre = ''
			if r ~= '' then
				r = r..stead.delim;
			end
			if v.n > 1 then
				pre = tostring(fprec(v.n))..txtnb(" ");
			end
			if k == urq.inv_selected then
				r = r..'<u><a:use '..k..'>'..pre..idisp(v.name)..'</a></u>'..inv_actions(k);
			else
				r = r..'<a:use '..k..'>'..pre..idisp(v.name)..'</a>';
			end
		end
	end
	return r;
end

function is_actions(o)
	local k,v
	if not o then
		return false
	end
	o = tostring(o)
	for k,v in pairs(urq.use_labels) do
		if k:find("use_"..o..'_', 1, true) then return true end
	end
	return false
end

function inv_actions(o)
	local k,v
	local r = ''
	k = "use_"..o;

	if urq.labels[k] then -- examine
		local orig = urq.orig_labels[k];
		local what = urq.examine
		if (not urq.extension_furq) or (not urq.vars[k.."_hide"]) or ( tonumber(urq.vars[k.."_hide"]) == 0) then
			r = r..stead.delim
			r = r ..'[ <a:act '..k..','..what..'>'..idisp(what)..'</a> ]'
		end
	end

	for k,v in pairs(urq.use_labels) do
		local s, e
		s, e = k:find("use_"..o..'_', 1, true) 
		if s then
			local orig = urq.orig_labels[k];
			local what = orig:sub(e + 1);
			if not what or what == '' then
				what = urq.examine
			end
			if (not urq.extension_furq) or (not urq.vars[k.."_hide"]) or ( tonumber(urq.vars[k.."_hide"]) == 0) then
				r = r..stead.delim
				r = r ..'[ <a:act '..k..','..what..'>'..idisp(what)..'</a> ]'
			end
		end
	end
--	if r ~= '' then
--		r = idisp(o)..':\n'..r;
--	end
	if r ~= r then r = r .. "\n"; end
	return r;
end

function urq_buttons()
	local ret = urq.buttons;
	if urq.paused then
		if urq.input then
			if not urq.extension_input_btn then
				return
			end
		else
			if not urq.extension_pause_btn then
				return
			end
		end
	end
	if ret == nil then return end
	ret = ret:gsub("^[ \t]*[\n]+","\n"):gsub("[\n]+$","\n");
	return ret;
end

function cancel_pause()
	timer:del();
	urq.paused = false;
	urq.branches = {};
	urq.input = nil
	urq.pause_ip = 0
end

function reset_pause()
	urq.ibranch = 0;
	if not urq.paused then
		urq.branches = {};
	else
		urq.output = urq.last_output
	end
end

function output_txt(r)
	if urq_buttons() then
		r = par("\n\n", r:gsub("[\n]+$",""), urq_buttons());
	end
	return r
end

function start_cmd()
	urq.output = ''
	urq.buttons = nil;
	urq.forgetprocs = false
	urq.btn_args = 1;
end

function reenter()
	local cur_loc = urq.cur_loc -- str["current_loc"]
	if not urq.extension_invproc or not cur_loc or urq.paused then return end
	local ip = urq.labels[cur_loc];
	start_cmd();
	urq.ro = true
	urq.last_loc = exec(ip);
	urq.last_loc = output_txt(urq.last_loc);
	urq.ro = false
	urq.output = nil
end

function do_common()
	local r=''
	local com = urq.vars["common"];
	if com and tonumber(com) ~= 0 then
		com = "common_"..tostring(com);
	else
		com = "common";
	end
	if urq.paused then
		com = urq.last_com;
	end
	if urq.labels[com] ~= nil and urq.str["last_btn_caption"] then -- not init
		r = exec(urq.labels[com]):gsub("[\n]+$","");
		r = output_txt(r);
	end
	urq.last_com = com;
	return r
end

iface.icmd = function(s, inp)
	local a = { };
	local cmd;
	local r = nil
	local rmode = false
	stead.cache = {}
	urq.fading = false
	cmd,a = stead.getcmd(inp);
	start_cmd()
--	print ("CMD:", inp);
	if cmd == "look" then
		if urq.last then
			return par('',urq.last, cat(urq.input,'\n'),"\n");
		end
-- first run
		reset_pause();
		r = exec(urq.ip);
		r = output_txt(r);
		urq.last_loc = r;
		urq.last_output = urq.output;
	elseif cmd == "inv" then
		r = inv();
	elseif cmd == "use" then
		if urq.paused then
			return
		end
		if urq.inv_selected == a[1] then -- unselect
			urq.inv_selected = nil
			return nil
		else
			if a[1]:find("^use_") and urq.labels[a[1]] then
				cmd = "act";
				urq.inv_selected = nil
			elseif is_actions(a[1]) then -- select
				urq.inv_selected = a[1];
				r = urq.last;
				return nil
			elseif urq.use_labels["use_"..a[1]] then -- use
				cmd = "act";
				a[1] = "use_"..a[1];
			else
				return nil, false
			end
		end
--		r = par("", inv_actions(a[1]));--, urq.last);
	elseif cmd == "way" then
		r = ''
	elseif cmd == "save" then
		r = game:save(unpack(a));
	elseif cmd == "load" then
		urq.fading = true
		r = game:load(unpack(a));
		r = par('', urq.last, cat(urq.input,'\n'));
	elseif cmd == "resume" then
		rmode = true
		cmd = a[1];
		a[1] = a[2];
	end
	if cmd == "click" then
		if not rmode and urq.paused then -- disable click while pause
			return
		end
		if urq.labels["image_click"] then
			cmd = "act"
			if tonumber(a[1]) ~= 1 then a[1] = 2 end -- only 2 btn mouse
			urq.vars["bmouse"] = tonumber(a[1]);
			urq.vars["xmouse"] = tonumber(a[2]);
			urq.vars["ymouse"] = tonumber(a[3]);
			a[1] = "image_click";
		end
	end
	if cmd == "act" then
		if not rmode and urq.paused then -- disable inv while pause
			return
		end
		reset_pause();
		urq.lastip  = urq.labels[a[1]];
		local e = exec(urq.lastip);
		r = output_txt(e);
		local last_act = urq.last_act
		if urq.buttons and not urq.paused then
			last_act = par('\n', urq.last_act, r);
		end
		reenter();
		r = par('\n', urq.last_loc, urq.last_act, r);
		urq.last_act = last_act
		urq.lastcmd = cmd
		urq.last_output = urq.output
	elseif cmd == "go" then
		local loc_go = false

		if urq.extension_furq and type(a[1]) == 'string' and a[1]:find("^[ \t]*[!%%]") then
			a[1] = a[1]:gsub("^[!%%][ \t]*", "");
			loc_go = true
		end

		reset_pause();
		if not rmode and inp:find(",") then -- user pressed
			local s,e = inp:find(",", 1, true);
			if not loc_go then
				urq.str["last_btn_caption"] = inp:sub(e+1);
			end
			cancel_pause();
			if not loc_go then
				urq.fading = true
			end
		end
		game:step();
		if not loc_go then
			r = do_common();
		end
		if not urq.forgetprocs then -- not in common!
			a[1] = urqbtnargs(a[1]);
			update_loc(a[1], not loc_go);
			if urq.extension_loccnt_btn then
				update_count(a[1]);
			end
			urq.lastip  = urq.labels[a[1]];
			r = exec(urq.lastip, a[1]);
			r = output_txt(r);
		end
		local rr = r
		if loc_go then
			rr = par('\n', urq.last_loc, r);
			urq.last_act = r
		else
			urq.last_loc = r;
			urq.last_act = nil;
		end
		urq.lastcmd = cmd
		urq.last_output = urq.output
		r = rr
	elseif cmd == "nop" then
		return
	end
--	print (r);
	if cmd == "go" or cmd == "look" or cmd == "use" or cmd == "act" then
		urq.last = r;
		r = par('', r, cat(urq.input,'\n'));
	end
--	urq.last_output = urq.output
	return cat(r, "\n");
end
local links = {}

local function postfmt(r, v)
	if type(r) == 'string' and v ~= false then
		r = r:gsub("<a:[^>]+>", function(s)
			local c = s:sub(4):gsub(">$", "")
			table.insert(links, c)
			return string.format("[%d] ", #links)
		end);
		r = r:gsub("</a>", "")
	end
	return r, v
end

iface.cmd = function(s, inp)
	local cmd, a = stead.getcmd(inp);
	local n = tonumber(cmd)
	if cmd == '' then inp = 'look' links = {} end
	if cmd == 'go' then
		return nil, false
	end
	local r, v
	if n and links[n] then
		local t = links[n]
		links = {}
		r = postfmt(iface.icmd(s, t))
	else
		r, v = postfmt(iface.icmd(s, inp))
	end
	if v == false then
		return r, v
	end
	return r
end
function resume()
	urq.ip = urq.lastip;
	timer:stop();
	urq.last = nil
	urq.labels["__resume_label__"] = urq.ip;
	return "resume "..urq.lastcmd..",__resume_label__";
end

timer.callback = function(s)
	timer:stop();
	return resume();
end

spec_keys = { ["up"] = 72, ["down"] = 80, ["left"] = 75, ["right"] = 77, ["\n"] = 13 }

function kbdcode(s)
	s = kbdxlat(s);
	local k = spec_keys[s:lower()] -- special keys
	if k then return k end
	if s:len() ~= 1 then
		return ''
	end
	return s:byte();
end

function kbdxlat(s)
	local kbd
	if s == 'space' then
		return ' '
	end
	if s == 'return' then
		return '\n'
	end
	if game.codepage == "cp1251" or game.codepage == "CP1251" then
		kbd = kbdwin;
	elseif game.codepage == "cp866" or game.codepage == "CP866" then
		kbd = kbddos;
	else
		kbd = kbdutf8;
	end

	if urq.kbd_alt then
		kbd = kbden;
	end

	if kbd and urq.kbd_shift then
		kbd = kbd.shifted;
	end
	if not kbd[s] then
		if urq.kbd_shift then
			return s:upper();
		end
		return s;
	end
	return kbd[s]
end

input.click = function(s, down, mb, x, y, px, py)
	if urq.input == '' then
		if down then
			if not urq.click_tm then
				urq.click_tm = get_ticks()
				return
			end
			if math.abs(get_ticks() - urq.click_tm) < 300 then
				urq.click_tm = nil
				urq.input = ' ';
				return resume();
			end
			urq.click_tm = get_ticks()
		end
		return
	end
	if not down or not px then
		return
	end
	return "click "..mb..','..px..','..py;
end

input.key = function(s, down, key)
	if key:find("shift") then
		urq.kbd_shift = down
	elseif key:find("alt") and down then
		urq.kbd_alt = not urq.kbd_alt;
	end

	if urq.last and urq.paused and urq.input ~= nil then
		if down and key=="escape" then
			return
		end
		if urq.input == '' and down then
			urq.input = kbdcode(key);
			if urq.input == '' then
				return
			end
			return resume();
		end
		if down and key == "return" then
			urq.input = urq.input:sub(1, urq.input:len()-urq.cursor:len());
			return resume();
		end
		local c = kbdxlat(key);
		if down and key~="escape" and (c:len() == 1 or key == "backspace") then
			urq.input = tostring(urq.input)
			urq.input = urq.input:sub(1, urq.input:len()-urq.cursor:len());
			if key == "backspace" then
				urq.input = urq.input:sub(1, urq.input:len()-1);
			else
				urq.input = par('', urq.input, kbdxlat(key));
			end
			urq.input = urq.input..urq.cursor;
			return "look";
		end
	end
end

main = room {
	nam = function(s)
		local l = urq.str["last_btn_caption"]
		if l then 
			l = l..'...'
		else
			l = urq.str["gametitle"]
		end
		if not l then l = '' end
		if urq.input then
			l = l .. ' (?)'
		elseif urq.paused then
			l = l .. ' (P)'
		end
		return l
	end,
	pic = function(s)
		if not urq.extension_varimage then return end
		if urq.str["image"] and urq.str["image"] ~= '' then return urq.str["image"] end
		local t = urq.vars["imagetype"]
		if not t then t = 0 end
		local i = urq.vars["image"]
		if not i or i == 0 then return end
		i = tostring(i)
		if t == 0 then
			i = i..'.bmp'
		elseif t == 6 then
			i = i..'.jpg'
		elseif t == 5 then
			i = i..'.gif'
		else
			i = i..'.png' -- heh ;)
		end
		return i
	end,
}

mlist = { 
	'back_again.mod',
	'four_stone_walls_-_chip_version.it',
	'jogeir-touchin_the_chip.xm',
	'music_for_chips.xm',
	'poobear.xm',
	'vibe-ew.it',
	'lms.xm',
}

mplayer = obj {
	nam = 'mplayer',
	_last = 0,
	life = function(s)
		if is_music() then
			return
		end
		local n = rnd(stead.table.maxn(mlist));
		while n == s._last do
			n = rnd(stead.table.maxn(mlist));
		end
		set_music("mus/"..mlist[n], 1);
		s._last = n;
	end
}

lifeon('mplayer');
mplayer:life();

orig_get_music = get_music;

get_music = function(s)
	if urq.str["music"] then
		if urq.str["music"] == '' then
			return
		end
		return urq.str["music"], 0
	end
	if urq.vars["music"] then
		if urq.vars["music"] == 0 then
			return
		end
		return tostring(urq.vars["music"])..'.mid', 0
	end
	return orig_get_music()
end

game.fading = function(s)
	local title = nameof(here());
	if title then title = title:gsub(" %(%?%)",""):gsub(" %(P%)", ""); end
	if game._time == 0 or urq.fading then
		game._last_title = title
		return true
	end
	if game._last_title ~= title then
		game._last_title = title
		return true
	end
	return false
end
if stead.version < "1.2.2" then
	stead.delim='\n';
else
	stead.delim='|'
end
-- game.codepage="CP1251"
