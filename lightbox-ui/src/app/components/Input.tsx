import React, { useId } from "react"
import { AlertCircle, Info } from "lucide-react"

export interface InputProps extends Omit<React.InputHTMLAttributes<HTMLInputElement>, "size"> {
  label?: string;
  error?: string;
  helper?: string;
  inputRef?: React.Ref<HTMLInputElement>;
  disableAutocomplete?: boolean;
  rightElement?: React.ReactNode;
  leftElement?: React.ReactNode;
  size?: "sm" | "md";
}

export const Input: React.FC<InputProps> = ({
  inputRef,
  label,
  error,
  helper,
  disableAutocomplete,
  rightElement,
  leftElement,
  size = "md",
  className = "",
  ...props
}) => {
  const sizeClasses = { sm: "px-2 py-1.5 text-sm", md: "px-2.5 py-2 text-sm" }
  const iconSize = { sm: "h-4 w-4", md: "h-4 w-4" }

  const inputClasses = `
    block w-full border border-black/10 dark:border-white/20 rounded-lg shadow-sm
    text-gray-700 placeholder-gray-600
    dark:bg-black/10 dark:text-gray-100 dark:placeholder-gray-300
    bg-radial-[at_35%_45%] form-white/10 to-black/10 dark:form-black/10 dark:to-white/10
    focus:outline-none focus:ring-2 ring-blue-500/50 focus:ring-offset-1 ring-offset-blue-400
    disabled:bg-black/20 dark:disabled:bg-white/20 dark:disabled:text-gray-50 disabled:cursor-not-allowed
    ${rightElement ? "pr-8" : ""}
    ${leftElement ? "pl-8" : ""}
    ${error ? "border-red-300 text-red-900 placeholder-red-300" : ""}
    ${sizeClasses[size]} ${className}
  `.replace(/\s+/gmi, " ").trim()

  if (disableAutocomplete) {
    props["autoComplete"] = "one-time-code"
  }

  const inputId = useId()
  return (
    <div className="space-y-1">
      {label && (
        <label htmlFor={inputId} className="block text-xs font-medium text-gray-700 dark:text-gray-300">
          {label}
        </label>
      )}
      <div className="relative">
        {leftElement && (
          <div className="absolute inset-y-0 left-0 pl-2.5 flex items-center">{leftElement}</div>
        )}
        <input id={inputId} ref={inputRef} className={inputClasses} {...props} />
        {rightElement && (
          <div className="absolute inset-y-0 right-0 pr-2.5 flex items-center">{rightElement}</div>
        )}
      </div>
      {error && (
        <div className="flex items-center gap-1 mt-1 text-sm text-red-600">
          <AlertCircle className={iconSize[size]}/>
          {error}
        </div>
      )}
      {helper && !error && (
        <p className="text-xs text-gray-600 dark:text-gray-400 flex gap-1">
          <Info size={14} className="text-blue-600 mt-0.5"/> {helper}
        </p>
      )}
    </div>
  )
}
