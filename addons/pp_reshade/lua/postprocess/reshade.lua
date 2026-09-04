local pp_reshade = CreateClientConVar("pp_reshade", "1", true, false)

local render = render
local function TryInstallHook()
	hook.Remove("RenderScreenspaceEffects", "DrawReShadeEffects")

	local DrawReShadeEffects = render.DrawReShadeEffects
	if not DrawReShadeEffects then return end

	if pp_reshade:GetBool() then
		hook.Add("RenderScreenspaceEffects", "DrawReShadeEffects", DrawReShadeEffects)
	end
end
TryInstallHook()
timer.Simple(0, TryInstallHook)
hook.Add("InitPostEntity", "ReShade_LateInit", TryInstallHook)
cvars.AddChangeCallback(pp_reshade:GetName(), TryInstallHook, pp_reshade:GetName())

list.Set("PostProcess", "#reshade_pp", {
	icon = "gui/postprocess/reshade.png",
	convar = pp_reshade:GetName(),
	category = "#shaders_pp",

	cpanel = function(CPanel)
		CPanel:Help("#reshade_pp.desc")
		CPanel:Help("#reshade_pp.desc2")

		CPanel:CheckBox("#reshade_pp.enable", pp_reshade:GetName())
	end
})