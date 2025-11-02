import React, { useId } from "react"
import { AlertCircle, Check } from "lucide-react"

export interface CheckboxProps extends Omit<React.InputHTMLAttributes<HTMLInputElement>, "type" | "size"> {
  label?: string;
  error?: string;
  helper?: string;
  inputRef?: React.Ref<HTMLInputElement>;
  size?: "sm" | "md";
}

export const Checkbox: React.FC<CheckboxProps> = ({
  inputRef,
  label,
  error,
  helper,
  size = "md",
  className = "",
  checked,
  ...props
}) => {
  const sizeClasses = { sm: "w-4 h-4", md: "w-5 h-5" }
  const labelSizeClasses = { sm: "text-xs", md: "text-sm" }

  const checkboxClasses = `
    peer appearance-none border border-black/10 dark:border-white/20 rounded shadow-sm
    bg-white/50 dark:bg-black/10
    checked:bg-blue-500 checked:border-blue-500 dark:checked:bg-blue-600 dark:checked:border-blue-600
    focus:outline-none focus:ring-2 ring-blue-500/50 focus:ring-offset-1 ring-offset-blue-400
    disabled:bg-black/20 dark:disabled:bg-white/20 disabled:cursor-not-allowed
    cursor-pointer transition-colors
    ${error ? "border-red-300" : ""}
    ${sizeClasses[size]} ${className}
  `.replace(/\s+/gmi, " ").trim()

  const checkIconClasses = `
    absolute pointer-events-none text-white
    opacity-0 peer-checked:opacity-100 transition-opacity
    ${sizeClasses[size]}
  `.replace(/\s+/gmi, " ").trim()

  const inputId = useId()

  return (
    <div className="space-y-1">
      <div className="flex items-start gap-2">
        <div className="relative flex items-center">
          <input id={inputId} ref={inputRef} type="checkbox" className={checkboxClasses} checked={checked}{...props}/>
          <Check className={checkIconClasses} strokeWidth={2}/>
        </div>
        {label && (
          <label
            htmlFor={inputId}
            className={`block font-medium text-gray-700 dark:text-gray-300 cursor-pointer select-none ${labelSizeClasses[size]}`}>
            {label}
          </label>
        )}
      </div>
      {error && (
        <div className="flex items-center gap-1 text-sm text-red-600 ml-7">
          <AlertCircle className="w-4 h-4"/>
          {error}
        </div>
      )}
      {helper && !error && (
        <p className="text-sm text-gray-600 dark:text-gray-400 ml-7">{helper}</p>
      )}
    </div>
  )
}
