import React, { useState } from "react"
import { Button } from "../../components/Button.tsx"
import { Settings, Wifi, WifiOff, Save, RotateCcw } from "lucide-react"
import { Popover, PopoverButton, PopoverPanel } from "@headlessui/react"
import { Input } from "../../components/Input.tsx"
import { Checkbox } from "../../components/Checkbox.tsx"
import { useMutation, useQueryClient, useSuspenseQuery } from "@tanstack/react-query"
import { api } from "../../lib/api"
import { ErrorMessage } from "../../components/ErrorMessage.tsx"
import { GenericSuspense } from "../../components/GenericSuspense.tsx"

const popoverTransitionClasses = "transition duration-200 ease-out data-closed:translate-y-10 " +
  "data-closed:scale-95 data-open:translate-y-0 data-closed:opacity-0"

const glassPanelClasses = "backdrop-blur-lg rounded-xl p-4 border bg-white/50 dark:bg-black/50 border-black/10 dark:border-white/20"

export const SettingsPanel: React.FC = () => {
  return (
    <>
      {/*<div className="hidden lg:block h-full">*/}
      {/*  <SuspenseWrapper/>*/}
      {/*</div>*/}
      <div>
        <Popover>
          <PopoverButton as="div">
            <Button size='sm'><Settings size={16}/> Settings</Button>
          </PopoverButton>
          <PopoverPanel
            anchor={{ "to": "top start", "gap": "10px", "padding": "10px" }}
            transition
            className={`${glassPanelClasses} ${popoverTransitionClasses} w-96 max-w-[90vw]`}>
            <SuspenseWrapper/>
          </PopoverPanel>
        </Popover>
      </div>
    </>
  )
}

function SuspenseWrapper() {
  return (
    <div className="flex flex-col h-full">
      <div className="flex items-center gap-2 mb-4 pr-2 pl-2">
        <Settings size={16} className="text-gray-700 dark:text-gray-300"/>
        <h2 className="text-sm font-semibold">Settings</h2>
      </div>

      <GenericSuspense>
        <SettingsControls/>
      </GenericSuspense>
    </div>
  )
}

const SettingsControls: React.FC = () => {
  const queryClient = useQueryClient()
  const { data } = useSuspenseQuery({
    staleTime: Infinity,
    queryKey: ["settings"],
    queryFn: api.getSettings,
  })
  type Config = typeof data;

  const updateConfig = useMutation({
    mutationFn: api.putSettings,
    onSuccess: (update) => queryClient.setQueryData(
      ["settings"], (prev: Config) => ({ ...prev, ...update })
    ),
  })


  const [config, setConfig] = useState<Config>({ ...data })
  const [showStationPassword, setShowStationPassword] = useState(false)
  const [showApPassword, setShowApPassword] = useState(false)

  const handleChange = <T extends keyof Config>(field: T, value: Config[T]) => {
    setConfig(prev => ({ ...prev, [field]: value }))
  }

  const handleSave = () => {
    updateConfig.mutate(config)
  }

  const handleReset = () => {
    setConfig({ ...data })
    updateConfig.reset()
  }

  return <>
    <div className="flex-1 space-y-4 pr-2 pl-2">
      <section className="space-y-2">
        <h3 className="text-sm font-semibold text-gray-700 dark:text-gray-300">Device Identity</h3>
        <Input
          type="text"
          label="Device Name"
          value={config.deviceName}
          onChange={(e) => handleChange("deviceName", e.target.value)}
          placeholder="Lightbox"
        />
        <div>
          <Input
            type="text"
            label="Hostname (mDNS)"
            value={config.hostname}
            onChange={(e) => handleChange("hostname", e.target.value.toLowerCase().replace(/[^a-z0-9-]/g, ""))}
            placeholder="lightbox"
            helper={`Access at: http://${config.hostname || "lightbox"}.local`}
            rightElement={<span className="text-xs text-gray-500 dark:text-gray-400">.local</span>}
          />
        </div>
      </section>

      <section className="space-y-2">
        <div className="flex items-center gap-2">
          <Wifi size={16} className="text-blue-500"/>
          <h3 className="text-sm font-semibold text-gray-700 dark:text-gray-300">Station Mode</h3>
        </div>
        <p className="text-xs text-gray-500 dark:text-gray-400">
          Connect to your existing WiFi network
        </p>

        <Checkbox
          size="sm"
          label="Enable Station Mode"
          checked={config.staEnabled}
          onChange={(e) => handleChange("staEnabled", e.target.checked)}
        />

        {config.staEnabled && (<>
          <Input
            type="text"
            label="WiFi SSID"
            value={config.staSsid}
            onChange={(e) => handleChange("staSsid", e.target.value)}
            placeholder="Your WiFi Network"
          />

          <Input
            type={showStationPassword ? "text" : "password"}
            label="WiFi Password"
            value={config.staPassword}
            onChange={(e) => handleChange("staPassword", e.target.value)}
            placeholder="Enter WiFi password"
            disableAutocomplete
            rightElement={config.staPassword ? (
              <Button
                type="button"
                size="xs"
                variant="ghost"
                onClick={() => setShowStationPassword(!showStationPassword)}
              >
                {showStationPassword ? "Hide" : "Show"}
              </Button>
            ) : undefined}
          />

          <Input
            type="number"
            label="Connection Timeout"
            value={config.staFallbackToApTimeout}
            onChange={(e) => handleChange("staFallbackToApTimeout", parseInt(e.target.value) || 0)}
            placeholder="30"
            min={5}
            max={300}
            helper={`If connection fails after ${config.staFallbackToApTimeout}s, switch to Access Point mode`}
            rightElement={<span className="text-xs text-gray-500 dark:text-gray-400">sec</span>}
          />
        </>)}
      </section>

      <section className="space-y-2">
        <div className="flex items-center gap-2">
          <WifiOff size={16} className="text-orange-500"/>
          <h3 className="text-sm font-semibold text-gray-700 dark:text-gray-300">Access Point Mode</h3>
        </div>
        <p className="text-xs text-gray-500 dark:text-gray-400">
          Lightbox creates its own WiFi network
        </p>

        <Input
          type="text"
          label="Access Point SSID"
          value={config.apSsid}
          onChange={(e) => handleChange("apSsid", e.target.value)}
          placeholder="Lightbox-AP"
          helper="Network name when in AP mode"
        />

        <Checkbox
          size="sm"
          label="Access Point SSID hidden"
          checked={config.apSsidHidden}
          onChange={(e) => handleChange("apSsidHidden", e.target.checked)}
        />

        <Input
          type={showApPassword ? "text" : "password"}
          label="Access Point Password"
          value={config.apPassword}
          onChange={(e) => handleChange("apPassword", e.target.value)}
          placeholder="Enter AP password"
          disableAutocomplete
          helper="Minimum 8 characters required"
          error={config.apPassword && config.apPassword.length < 8 ? "Password must be at least 8 characters" : undefined}
          rightElement={config.apPassword ? (
            <Button
              type="button"
              size="xs"
              variant="ghost"
              onClick={() => setShowApPassword(!showApPassword)}
            >
              {showApPassword ? "Hide" : "Show"}
            </Button>
          ) : undefined}
        />

        <Checkbox
          size="sm"
          label="Access Point always enabled"
          checked={config.apAlwaysEnabled}
          onChange={(e) => handleChange("apAlwaysEnabled", e.target.checked)}
        />
      </section>
    </div>

    <div className="flex flex-col gap-2 mt-4 mx-2 pt-4 border-t border-gray-400 dark:border-gray-500">
      {updateConfig.isError && (
        <ErrorMessage className="line-clamp-2 mb-2">{updateConfig.error.message}</ErrorMessage>)}
      <div className="flex gap-2">
        <Button onClick={handleSave} className="flex-1" loading={updateConfig.isPending}>
          <Save size={14}/> Save Configuration
        </Button>
        <Button onClick={handleReset}>
          <RotateCcw size={14}/>
        </Button>
      </div>
    </div>
  </>
}
