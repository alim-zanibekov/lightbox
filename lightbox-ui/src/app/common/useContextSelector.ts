import { useContext, useMemo, type Context } from "react"

export function useContextSelector<C, S>(
  context: Context<C>,
  selector: (value: C) => S
): S {
  const value = useContext(context)
  const selected = selector(value)

  return useMemo(() => selected, [selected])
}
