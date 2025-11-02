import React, { useEffect, useMemo, useState } from "react"
import { ArrowUpFromLine, Images, Loader2, Save } from "lucide-react"
import { Button } from "../../components/Button.tsx"
import { Dialog, DialogPanel, DialogTitle, Popover, PopoverButton, PopoverPanel } from "@headlessui/react"
import { Trash2 } from "lucide-react"
import { useMutation, useQueryClient, useSuspenseQuery } from "@tanstack/react-query"
import { updateAppStore, useAppStore } from "../../store/appStore.ts"
import { api } from "../../lib/api"
import { ErrorMessage } from "../../components/ErrorMessage.tsx"
import type { Gallery } from "../../lib/api/LightboxApi.ts"
import { GenericSuspense } from "../../components/GenericSuspense.tsx"

const maxItems = 20

const glassPanelClasses = "backdrop-blur-lg rounded-xl p-4 border bg-white/50 dark:bg-black/50 border-black/10 dark:border-white/20"
const glassPanelClassesModal = "backdrop-blur-lg rounded-xl p-4 border bg-black/20 dark:bg-white/20 border-black/10 dark:border-white/20"
const popoverTransitionClasses = "transition duration-200 ease-out data-closed:translate-y-10 " +
  "data-closed:scale-95 data-open:translate-y-0 data-closed:opacity-0"


export const GalleryPanel: React.FC = () => {
  return (
    <>
      <div className="hidden lg:block h-full">
        <SuspenseWrapper/>
      </div>
      <div className="lg:hidden">
        <Popover>
          <PopoverButton as="div">
            <Button size='sm'><Images size={16}/> Images</Button>
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
    <div className="w-full">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2 mb-4">
          <Images size={16} className="text-gray-700 dark:text-gray-300"/>
          <h2 className="text-sm font-semibold">Images</h2>
        </div>
      </div>
      <GenericSuspense>
        <GalleryControls/>
      </GenericSuspense>
    </div>
  )
}

function GalleryControls() {
  const queryClient = useQueryClient()
  const { data } = useSuspenseQuery({
    staleTime: Infinity,
    queryKey: ["gallery"],
    queryFn: api.getGallery,
  })

  const addItem = useMutation({
    mutationFn: api.postGallery,
    onSuccess: (item) => queryClient.setQueryData(
      ["gallery"], (prev: Gallery) => ({ ...prev, items: [...prev.items, item] }),
    ),
  })

  const deleteAll = useMutation({
    mutationFn: () => api.deleteGallery(),
    onSuccess: () => queryClient.setQueryData(
      ["gallery"], (prev: Gallery) => ({ ...prev, items: [] }),
    ),
  })

  const deleteOne = useMutation({
    mutationFn: async (index: number) => {
      await api.deleteGallery(index)
      return index
    },
    onSuccess: (idx) => queryClient.setQueryData(
      ["gallery"], (prev: Gallery) =>
        ({ ...prev, items: prev.items.filter((_, i) => i !== idx) }),
    ),
  })

  useEffect(() => {
    updateAppStore({ gallery: data })
  }, [data])

  const { gallery, matrix } = useAppStore((s) => ({
    gallery: s.gallery ?? data,
    matrix: s.matrix
  }))

  const lastSubmitted = [addItem, deleteAll, deleteOne]
    .sort((a, b) => a.submittedAt - b.submittedAt)
    .pop()

  return <>
    <div className="overflow-x-scroll pb-4 -mx-4">
      {gallery.items.length === 0 ? (
        <div
          className="col-span-5 p-4 mx-4 rounded-lg border border-dashed border-gray-700 dark:border-gray-300 text-xs text-gray-700 dark:text-gray-300">
          No saved images yet — use <strong>Save</strong> to store up to {maxItems} previews.
        </div>
      ) : <div className="px-4 w-max">
        <div className="flex items-center justify-start gap-2">
          {gallery.items.map((m, idx) => ({ idx, m })).reverse().map(({ idx, m }) => (
            <SavedItemCard
              name={m.name}
              key={idx}
              matrix={m.matrix}
              onDelete={() => deleteOne.mutate(idx)}
              isDeleteLoading={deleteOne.variables === idx && deleteOne.isPending}
              isDeleteDisabled={deleteOne.isPending || addItem.isPending || deleteAll.isPending}
            />
          ))}
        </div>
      </div>}

    </div>
    <div>
      <div className="flex items-center justify-between gap-2 mt-1">
        <div className="text-xs text-gray-500 dark:text-gray-400">{gallery.items.length} / {gallery.capacity}</div>
        <div className="flex items-center gap-2">
          <Button
            loading={addItem.isPending}
            disabled={deleteAll.isPending || deleteOne.isPending}
            size="sm"
            onClick={() => {
              if (!matrix) return
              const num = gallery.items.map(it => +it.name.split(" ").pop()!)
                .filter(it => !!it)
                .pop() || 0
              addItem.mutate({ matrix, name: "Item " + (num + 1) })
            }}>
            <Save size={14}/>Save
          </Button>
          <Button
            loading={deleteAll.isPending}
            disabled={addItem.isPending || deleteOne.isPending}
            size="sm" variant="danger"
            onClick={() => deleteAll.mutate()}>
            <Trash2 size={14}/>Delete All
          </Button>
        </div>
      </div>
      {lastSubmitted && lastSubmitted.isError && (
        <ErrorMessage className="line-clamp-2 mt-2 ">{lastSubmitted.error.message}</ErrorMessage>)}
    </div>
  </>

}

function SavedItemCard({
  name,
  matrix,
  onDelete,
  isDeleteLoading,
  isDeleteDisabled
}: {
  name: string
  matrix: string[][]
  onDelete: () => void
  isDeleteLoading: boolean
  isDeleteDisabled: boolean
}) {
  const [showPreview, setShowPreview] = useState(false)
  const rows = matrix.length
  const cols = matrix[0]?.length || 0

  const cellSize = 7
  const gap = 1

  const svgMarkup = useMemo(() => {
    const cg = cellSize + gap
    const rects = matrix
      .map((row, r) =>
        row.map((color, c) =>
          `<rect x="${c * cg}" y="${r * cg}" width="${cellSize}" height="${cellSize}" fill="${color}" />`
        ).join("")
      ).join("")
    return (
      `<svg xmlns="http://www.w3.org/2000/svg"
            width="${cols * cg}" height="${rows * cg}"
            viewBox="0 0 ${cols * cg} ${rows * cg}">
        ${rects}
      </svg>`
    )
  }, [matrix, rows, cols])

  const svgUrl = useMemo(() => {
    return `data:image/svg+xml;base64,${btoa(svgMarkup)}`
  }, [svgMarkup])

  const aspectRatio = cols / rows

  const imageClasses = aspectRatio < 1
    ? "h-[70dvh]"
    : "w-[70dvw]"

  return (
    <>
      <div
        className="min-w-35 bg-white/5 dark:bg-white/10 rounded-md border border-gray-200 dark:border-gray-700 p-2 shadow-sm">
        <div className="flex items-center justify-between z-20 relative">
          <div className="text-xs font-medium text-gray-700 dark:text-gray-300 line-clamp-1 truncate">{name}</div>
          <div className="flex gap-0.5 pl-1">
            <Button
              variant="ghost" size="xs"
              title="Apply"
              onClick={() => {
                updateAppStore({ matrix })
              }}>
              <ArrowUpFromLine size={14} className="text-blue-500"/>
            </Button>
            <Button
              variant="ghost" size="xs"
              disabled={isDeleteDisabled}
              title="Delete"
              onClick={() => onDelete()}>
              {isDeleteLoading ? <Loader2 size={14} className="text-red-500 animate-spin"/> :
                <Trash2 size={14} className="text-red-500"/>}
            </Button>
          </div>
        </div>
        <div className="mt-2 flex justify-center items-center cursor-pointer"
             onClick={() => setShowPreview(true)}>
          <img src={svgUrl} alt={name}/>
        </div>
      </div>
      <Dialog
        open={showPreview}
        onClose={() => setShowPreview(false)}
        className="fixed inset-0 z-50 flex items-center justify-center">
        <div className="fixed inset-0 bg-black/60 dark:bg-black/80"/>
        <DialogPanel
          className={`${glassPanelClassesModal} relative max-w-[90vw] max-h-[90vh] flex flex-col`}>
          <DialogTitle
            className="text-gray-300 mb-4 text-base font-semibold flex justify-between items-center">
            <span>{name}</span>
            <Button
              variant="ghost" size="xs"
              title="Close"
              onClick={() => setShowPreview(false)}
              className="text-gray-100! hover:text-gray-100! text-xl! px-2! py-0.5!">
              ✕
            </Button>
          </DialogTitle>
          <div
            className="overflow-auto flex items-center justify-center bg-white/10 dark:bg-black/20 rounded-lg p-4">
            <img src={svgUrl} alt={name} className={`${imageClasses} object-contain`}/>
          </div>
        </DialogPanel>
      </Dialog>
    </>
  )
}

