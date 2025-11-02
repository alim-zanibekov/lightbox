import React, { useState } from "react"
import { Button } from "../../components/Button.tsx"
import { Save, Cpu } from "lucide-react"
import { Popover, PopoverButton, PopoverPanel } from "@headlessui/react"
import { useMutation, useSuspenseQuery } from "@tanstack/react-query"
import { api } from "../../lib/api"
import { ErrorMessage } from "../../components/ErrorMessage.tsx"
import { GenericSuspense } from "../../components/GenericSuspense.tsx"
import { Input } from "../../components/Input.tsx"

const popoverTransitionClasses = "transition duration-200 ease-out data-closed:translate-y-10 " +
  "data-closed:scale-95 data-open:translate-y-0 data-closed:opacity-0"

const glassPanelClasses = "backdrop-blur-lg rounded-xl p-4 border bg-white/50 dark:bg-black/50 border-black/10 dark:border-white/20"

export const FirmwarePanel: React.FC = () => {
  return (
    <>
      {/*<div className="hidden lg:block h-full">*/}
      {/*  {element}*/}
      {/*</div>*/}
      <div
      >
        <Popover>
          <PopoverButton as="div">
            <Button size='sm'><Cpu size={16}/> Firmware</Button>
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
        <Cpu size={16} className="text-gray-700 dark:text-gray-300"/>
        <h2 className="text-sm font-semibold">Device Firmware</h2>
      </div>

      <GenericSuspense>
        <FirmwareControls/>
      </GenericSuspense>
    </div>
  )
}

const FirmwareControls: React.FC = () => {

  const { data: firmwareInfo } = useSuspenseQuery({
    staleTime: Infinity,
    queryKey: ["appInfo"],
    queryFn: api.getFirmwareInfo,
  })

  const uploadOTA = useMutation({
    mutationFn: (it: { file: File, onProgress?: (progress: number) => void }) => api.otaUpdate(it.file, it.onProgress),
  })

  const [selectedFile, setSelectedFile] = useState<File | null>(null)
  const [progress, setProgress] = useState<number | null>(null)
  const [countdown, setCountdown] = useState<number | null>(null)

  React.useEffect(() => {
    if (uploadOTA.isSuccess) {
      setCountdown(10)
      const timer = setInterval(() => {
        setCountdown(prev => {
          if (prev === 1) {
            clearInterval(timer)
            window.location.reload()
          }
          return prev ? prev - 1 : null
        })
      }, 1000)
      return () => clearInterval(timer)
    }
  }, [uploadOTA.isSuccess])

  return <>
    <div className="flex-1 space-y-4 pr-2 pl-2">
      <section className="space-y-2">
        <h3 className="text-sm font-semibold text-gray-700 dark:text-gray-300">Info</h3>

        <div className="text-sm space-y-1">
          <div className="flex justify-between">
            <span className="text-gray-600 dark:text-gray-400">Version:</span>
            <span className="font-mono">{firmwareInfo.version}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-600 dark:text-gray-400">Compile Date:</span>
            <span className="font-mono">{firmwareInfo.compileDate}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-600 dark:text-gray-400">Compile Time:</span>
            <span className="font-mono">{firmwareInfo.compileTime}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-600 dark:text-gray-400">Hash:</span>
            <span className="font-mono text-xs" title={firmwareInfo.hashSHA256}>
              {firmwareInfo.hashSHA256.slice(0, 16)}...
            </span>
          </div>
        </div>
      </section>

      <section className="space-y-2">
        <h3 className="text-sm font-semibold text-gray-700 dark:text-gray-300">Firmware Update</h3>

        <Input
          type="file"
          size="sm"
          className="block w-full text-sm text-gray-500 file:mr-4 file:py-2 file:px-4
            file:rounded-md file:border-0 file:text-sm file:font-semibold
            file:bg-white/20 file:text-gray-700 hover:file:bg-white/40
            dark:file:bg-blue-900/20 dark:file:text-blue-500 dark:hover:file:bg-blue-900/50"
          onChange={(e) => setSelectedFile(e.target.files?.[0] || null)}
          accept=".bin">

        </Input>

        {uploadOTA.isError && (
          <ErrorMessage className="line-clamp-2 mb-2">{uploadOTA.error.message}</ErrorMessage>)}

        <div className="flex gap-2">
          <Button
            onClick={() => {
              if (selectedFile) {
                setProgress(null)
                uploadOTA.mutate({
                  file: selectedFile,
                  onProgress: (progress) => {
                    setProgress(progress)
                  }
                })
              }
            }}
            disabled={!selectedFile || countdown !== null}
            className="flex-1"
            loading={uploadOTA.isPending}>
            <Save size={14}/> Upload Firmware
          </Button>
        </div>

        {uploadOTA.isPending && progress !== null && (
          <div className="space-y-1 pt-1">
            <div className="h-2 w-full bg-gray-500/50 rounded-full overflow-hidden">
              <div
                className="h-full bg-blue-500 transition-all duration-300"
                style={{ width: `${progress}%` }}
              />
            </div>
            <p className="text-xs text-gray-600 dark:text-gray-400 text-right">{progress}%</p>
          </div>
        )}

        {countdown !== null && (
          <div className="text-sm text-center text-gray-600 dark:text-gray-400">
            Reloading in {countdown} seconds...
          </div>
        )}
      </section>
    </div>
  </>
}
